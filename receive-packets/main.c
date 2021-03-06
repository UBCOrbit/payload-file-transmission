#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <crc32.h>
#include <protocol.h>
#include <sha256_utils.h>



#define RECEIVING_FILE "receiving.meta"
//#define RECEIVED_PACKETS_DIR "received-packets"

void readAllOrDie(int fd, uint8_t *buf, size_t len)
{
    size_t offset = 0;
    ssize_t result;
    while (offset < len) {
        result = read(fd, buf + offset, len - offset);
        if (result == -1) {
            perror("Error reading file descriptor");
            exit(-1);
        }
        offset += result;
    }
}

void readHeader(int serialfd, uint8_t shaSum[32], size_t *fileSize, size_t *numPackets)
{
    uint8_t command;
    uint8_t inBuf[48];
    ssize_t result;

    // Header format:
    //  * 1 byte for TRANSFER_START
    //  * 8 bytes for file size in bytes
    //  * 8 bytes for number of packets
    //  * 32 bytes for sha256sum

    result = read(serialfd, &command, 1);
    if (result == -1) {
        perror("Error reading header command");
        exit(-1);
    }

    if (command != TRANSFER_START) {
        printf("Recieved erroneous command instead of TRANSFER_START\n");
        exit(-1);
    }

    readAllOrDie(serialfd, inBuf, 48);

    //*fileSize = *((uint64_t*) (inBuf + 0));
    //*numPackets = *((uint64_t*) (inBuf + 8));
    memcpy(fileSize, inBuf + 0, 8);
    memcpy(numPackets, inBuf + 8, 8);
    memcpy(shaSum, inBuf + 16, 32);
}

void createMetadataFile(uint8_t shaSum[32], size_t fileLen, size_t packetNum)
{
    char shaStr[65];
    FILE *metafp;

    sha256Str(shaStr, shaSum);

    // TODO: check if the file exists already to handle resuming transfers
    //       otherwise this file would be pointless
    metafp = fopen(RECEIVING_FILE, "w");
    if (metafp == NULL) {
        perror("Error creating recieving metadata file");
        exit(-1);
    }

    fprintf(metafp, "%s\n%zu\n%zu\n", shaStr, fileLen, packetNum);
    fclose(metafp);
}

void readPacketHeader(int serialfd, uint16_t *packetLen, uint32_t *crcSum)
{
    uint8_t pktCommand = 0;
    ssize_t pktRes = read(serialfd, &pktCommand, 1);
    if (pktRes == -1) {
        perror("Error reading packet command");
        exit(-1);
    }

    if (pktCommand == TRANSFER_END) {
        printf("Recieved a premature transfer_end command.\n");
        exit(-1);
    } else if (pktCommand != TRANSFER_PACKET) {
        printf("Recieved erroneous command instead of transfer_packet.\n");
        exit(-1);
    }

    // read packet header
    uint8_t pktHeader[6];
    readAllOrDie(serialfd, pktHeader, 6);

    //*packetLen = *((uint16_t *) (pktHeader + 0));
    //*crcSum    = *((uint32_t *) (pktHeader + 2));
    memcpy(packetLen, pktHeader + 0, 2);
    memcpy(crcSum, pktHeader + 2, 4);
}

uint8_t* readPacket(int serialfd, size_t packetLen)
{
    uint8_t *data = malloc(packetLen);
    readAllOrDie(serialfd, data, packetLen);

    return data;
}

void replyCommand(int serialfd, uint8_t command)
{
    ssize_t result = write(serialfd, &command, 1);
    if (result == -1) {
        perror("Error writing reply command");
        exit(-1);
    }
}

int main(int argc, char **argv)
{
    char dir[1024] = "";
    size_t start = 0;

    int c = 0;
    while (true) {
        static struct option long_options[] = {
            {"directory", required_argument, 0, 'd'},
            {"start",     required_argument, 0, 's'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "d:s:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'd':
                strncpy(dir, optarg, sizeof(dir));
                break;
            case 's':
                start = strtoul(optarg, NULL, 0);
                break;
        }
    }

    if (optind == argc) {
        printf("%s expects a serial device to communicate across\n", argv[0]);
        exit(-1);
    }

    size_t fileLen, packetNum;
    uint8_t shaSum[32];
    int serialfd;

    serialfd = open(argv[1], O_RDWR);
    if (serialfd == -1) {
        perror("Error opening serial device");
        exit(-1);
    }

    readHeader(serialfd, shaSum, &fileLen, &packetNum);
    createMetadataFile(shaSum, fileLen, packetNum);

    // Debug info
    printf("Received header, listening for packets...\n\n");

    // Read the packets
    for (size_t i = start; i < packetNum;) {
        uint16_t packetLen;
        uint32_t crcSum;
        uint8_t *data;

        // Debug info
        printf("Listening for packet%zu...\n", i);

        readPacketHeader(serialfd, &packetLen, &crcSum);

        // Debug info
        printf("\tReceived header: %u, %u\n", packetLen, crcSum);

        data = readPacket(serialfd, packetLen);

        // Debug info
        printf("\tReceived packet data, sending reply...\n");

        // calculate the crc32sum on this end to verify packet integrity
        if (crcSum != crc32(data, packetLen)) {
            // If the calculated crc32sum differs from given,
            // request that the packet is sent again
            printf("Error receiving packet: calculated crc32sum differs from given.\n");
            replyCommand(serialfd, TRANSFER_AGAIN);
            free(data);
            continue;
        }

        replyCommand(serialfd, TRANSFER_NEXT);

        // Debug info
        printf("Packet intact, writing out to file\n");

        char packetPath[1024];
        strncpy(packetPath, dir, sizeof(packetPath));
        strncat(packetPath, "/", sizeof(packetPath) - strlen(packetPath) - 1);
        snprintf(packetPath + strlen(packetPath), sizeof(packetPath) - strlen(packetPath),
                 "%zu.pkt", i);

        FILE *packetfp = fopen(packetPath, "w");
        if (packetfp == NULL) {
            perror("Error opening packet file for write");
            exit(-1);
        }

        fwrite(data, 1, packetLen, packetfp);
        fclose(packetfp);
        free(data);

        i += 1;
    }

    close(serialfd);
}
