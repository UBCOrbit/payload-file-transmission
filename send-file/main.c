#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <crc32.h>
#include <protocol.h>
#include <sha256_utils.h>



long fileLength(FILE *fp)
{
    if (fseek(fp, 0, SEEK_END) == -1) {
        perror("Error seeking file end");
        exit(-1);
    }

    long len = ftell(fp);
    rewind(fp);

    return len;
}

uint8_t* readFile(FILE *fp, size_t *size)
{
    long len = fileLength(fp);

    uint8_t *data = malloc(len);
    size_t read = fread(data, 1, len, fp);
    if (read != len) {
        printf("Error reading file\n");
        exit(-1);
    }
    fclose(fp);

    *size = len;
    return data;
}

void writeAllOrDie(int fd, const uint8_t *data, size_t dataLen)
{
    size_t written = 0;
    ssize_t result;

    while (written < dataLen) {
        result = write(fd, data + written, dataLen - written);
        if (result == -1) {
            perror("Error writing data to file descriptor");
            exit(-1);
        }
        written += result;
    }
}

void writeHeader(int serialfd, const uint8_t shaSum[32], size_t fileLen, size_t numPackets)
{
    uint64_t fileLen_64, numPackets_64;
    uint8_t outBuf[49];

    // Header format:
    //  * 1 byte for TRANSFER_START
    //  * 8 bytes for file size in bytes
    //  * 8 bytes for number of packets
    //  * 32 bytes for sha256sum

    fileLen_64 = fileLen;
    numPackets_64 = numPackets;

    outBuf[0] = TRANSFER_START;
    memcpy(outBuf + 1, &fileLen_64, 8);
    memcpy(outBuf + 9, &numPackets_64, 8);
    memcpy(outBuf + 17, shaSum, 32);

    writeAllOrDie(serialfd, outBuf, 49);
}

void writePacket(int serialfd, const uint8_t *packetData, size_t packetLen)
{
    uint16_t packetLen_16;
    uint32_t crcSum;
    uint8_t *outBuf;

    // Packet format:
    //  * 1 byte for TRANSFER_PACKET
    //  * 2 bytes for packet size in bytes
    //  * 4 bytes for crc32sum
    //  * n bytes for packet data

    packetLen_16 = packetLen;
    crcSum = crc32(packetData, packetLen);

    outBuf = malloc(7 + packetLen);

    outBuf[0] = TRANSFER_PACKET;
    memcpy(outBuf + 1, &packetLen_16, 2);
    memcpy(outBuf + 3, &crcSum, 4);
    memcpy(outBuf + 7, packetData, packetLen);

    // Debug info
    printf("\tSending packet: %u, %u\n", packetLen_16, crcSum);

    writeAllOrDie(serialfd, outBuf, 7 + packetLen);

    free(outBuf);
}

bool readResponse(int serialfd)
{
    // if response is TRANSFER_NEXT return true
    // if response is TRANSFER_AGAIN return false
    // if response is an error, exit?

    uint8_t response;
    ssize_t result;

    result = read(serialfd, &response, 1);
    if (result == -1) {
        perror("Error reading packet response");
        exit(-1);
    }

    switch (response) {
        case TRANSFER_NEXT:
            return true;
            break;
        case TRANSFER_AGAIN:
            return false;
            break;
        case TRANSFER_END:
            printf("Received premature TRANSFER_END response\n");
            exit(-1);
        case TRANSFER_ERROR:
            printf("Received TRANSFER_ERROR response\n");
            exit(-1);
        default:
            printf("Received erroneous transfer response\n");
            exit(-1);
    }
}

void writeProgress(char *filePath, size_t packet_index)
{
    FILE *fp = fopen("next-packet-out", "w");
    fprintf(fp, "%s\n%zu\n", filePath, packet_index);
    fclose(fp);
}

void eraseProgress()
{
    remove("next-packet-out");
}

int main(int argc, char **argv)
{
    FILE *file = stdin;
    size_t start = 0;

    int c = 0;
    while (true) {
        static struct option long_options[] = {
            {"file",  required_argument, 0, 'f'},
            {"start", required_argument, 0, 's'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "f:s:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'f':
                file = fopen(optarg, "r");
                if (file == NULL) {
                    perror("Error opening file");
                    exit(-1);
                }
                break;
            case 's':
                start = strtoul(optarg, NULL, 0);
                break;
        }
    }

    if (optind == argc) {
        printf("%s expects a serial port to communicate across\n", argv[0]);
        return -1;
    }

    size_t fileLen, packetNum;
    uint8_t *fileData;
    int serialfd;

    fileData = readFile(file, &fileLen);
    packetNum = fileLen / PACKET_SIZE + (fileLen % PACKET_SIZE == 0 ? 0 : 1);

    if (start >= packetNum) {
        printf("Given a start packet that is greater than the total number of packets for that file\n");
        exit(-1);
    }

    serialfd = open(argv[optind], O_RDWR);
    if (serialfd == -1) {
        perror("Error opening serial port");
        exit(-1);
    }

    // parse progress here

    // If this is the start of the transfer, write out the header
    if (start == 0) {
        uint8_t shaSum[32];
        char shaStr[65];

        calculateSHA256(fileData, fileLen, shaSum);
        sha256Str(shaStr, shaSum);

        writeHeader(serialfd, shaSum, fileLen, packetNum);

        // Debug info
        printf("Header written, sending packets...\n");
    } else {
        // Debug info
        printf("Resuming transfer at packet %zu\n", start);
    }

    for (size_t i = start; i < packetNum;) {
        size_t offset = i * PACKET_SIZE;
        size_t packetLen = fileLen - offset > PACKET_SIZE ? PACKET_SIZE : fileLen - offset;

        // Debug info
        printf("Sending packet %zu\n", i);

        writePacket(serialfd, fileData + offset, packetLen);
        if (readResponse(serialfd))
            i += 1;
    }

    free(fileData);
    eraseProgress();
}
