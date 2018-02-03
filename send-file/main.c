#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
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

uint8_t* readFile(char *filePath, size_t *size)
{
    FILE *fp = fopen(filePath, "r");
    if (fp == NULL) {
        perror("Error opening file");
        exit(-1);
    }

    long len = fileLength(fp);

    uint8_t *data = malloc(len);
    size_t read = fread(data, 1, len, fp);
    if (read != len) {
        printf("Error reading file %s.\n", filePath);
        exit(-1);
    }
    fclose(fp);

    *size = len;
    return data;
}

void writeMetadataFile(char shaStr[65], const char *filename)
{
    char *metaPath = "sending.meta";

    FILE *metafp = fopen(metaPath, "w");
    if (metafp == NULL) {
        perror("Error opening metadata file");
        exit(-1);
    }

    fprintf(metafp, "%s\n%s\n", shaStr, filename);
    fclose(metafp);
}

void deleteMetadataFile()
{
    if (remove("sending.meta") != 0) {
        perror("Error removing sending metadata file");
        exit(-1);
    }
}

void writeAllOrDie(int fd, const uint8_t *data, size_t dataLen)
{
    size_t written = 0;
    ssize_t result;

    while (written < dataLen) {
        result = write(fd, data, dataLen - written);
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
            printf("Recieved premature TRANSFER_END response\n");
            exit(-1);
        case TRANSFER_ERROR:
            printf("Recieved TRANSFER_ERROR response\n");
            exit(-1);
        default:
            printf("Recieved erroneous transfer response\n");
            exit(-1);
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("%s expects a serial port and a file to send across.\n", argv[0]);
        return -1;
    }

    size_t fileLen, packetNum;
    uint8_t *fileData;
    uint8_t shaSum[32];
    char shaStr[65];
    int serialfd;

    // Write out the file metadata
    // TODO: check if a metadata already exists to send a specific error response
    fileData = readFile(argv[2], &fileLen);

    calculateSHA256(fileData, fileLen, shaSum);
    sha256Str(shaStr, shaSum);

    writeMetadataFile(shaStr, argv[2]);

    // Send the packets over the serial port
    serialfd = open(argv[1], O_RDWR);
    if (serialfd == -1) {
        perror("Error opening serial port");
        exit(-1);
    }

    packetNum = fileLen / PACKET_SIZE + (fileLen % PACKET_SIZE == 0 ? 0 : 1);

    writeHeader(serialfd, shaSum, fileLen, packetNum);

    for (size_t i = 0; i < packetNum;) {
        size_t offset = i * PACKET_SIZE;
        size_t packetLen = fileLen - offset > PACKET_SIZE ? PACKET_SIZE : fileLen - offset;

        writePacket(serialfd, fileData + offset, packetLen);
        if (readResponse(serialfd))
            i += 1;
    }

    free(fileData);

    deleteMetadataFile();
}
