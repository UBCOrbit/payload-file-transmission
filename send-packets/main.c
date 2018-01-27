#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <linux/limits.h>

#include <crc32.h>

#define STORE_DIR "../packet-test"

enum TransferCommands {
    TRANSFER_START = 1,
    TRANSFER_PACKET,
    TRANSFER_NEXT,
    TRANSFER_AGAIN,
    TRANSFER_END,
    TRANSFER_ERROR
};

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("%s expects a sha256 sum and a file as arguments\n", argv[0]);
        return 1;
    }

    if (strlen(argv[1]) != 64) {
        printf("%s is not a valid sha256sum.\n", argv[1]);
        return 1;
    }

    uint8_t shaSum[32];
    for (size_t i = 0; i < 32; ++i) {
        char byteStr[3];
        byteStr[0] = argv[1][2 * i];
        byteStr[1] = argv[1][2 * i + 1];
        byteStr[2] = '\0';

        char *test;
        shaSum[i] = strtoul(byteStr, &test, 16);
        if (test != byteStr + 2) {
            printf("Error parsing sha256sum: %s. Unexpected `%c'\n", argv[1], *test);
            return 1;
        }
    }

    char *shaStr = argv[1];

    char pktdir[PATH_MAX] = STORE_DIR;
    strcat(pktdir, "/");
    strcat(pktdir, shaStr);

    // Debug info
    printf("%s\n", pktdir);

    // Check if the directory exists
    struct stat st;
    if (stat(pktdir, &st) == -1) {
        perror("stat");
        return 1;
    } else if (!S_ISDIR(st.st_mode)) {
        printf("Error: %s exists but is not a directory.\n", shaStr);
        return 1;
    }

    char metapath[PATH_MAX];
    strcpy(metapath, pktdir);
    strcat(metapath, ".meta");

    // Open the metadata file
    FILE *metap = fopen(metapath, "r");
    if (metap == NULL) {
        perror("fopen");
        return 1;
    }

    // Parse the metadata file
    size_t fileSize = 0;
    size_t packetNum = 0;

    char *line = NULL;
    size_t lineSize = 0;
    ssize_t lineLen = getline(&line, &lineSize, metap);
    if (lineLen == -1) {
        perror("getline");
        return 1;
    }
    fileSize = strtoul(line, NULL, 10);

    lineLen = getline(&line, &lineSize, metap);
    if (lineLen == -1) {
        perror("getline");
        return 1;
    }
    packetNum = strtoul(line, NULL, 10);

    free(line);
    fclose(metap);

    if (fileSize == 0 || packetNum == 0) {
        printf("Error parsing metadata file: %s.\n", metapath);
        return 1;
    }

    // Open the file descriptor to send data across
    int serialfd = open(argv[2], O_RDWR);
    if (serialfd == -1) {
        perror("open");
        return 1;
    }

    // construct file transmission header
    //   1 byte for command (techincally not part of header)
    //   8 bytes for file size in bytes
    //   8 bytes for number of packets
    //   32 bytes for sha256sum
    uint8_t header[49];

    header[0] = TRANSFER_START;

    uint64_t size = fileSize;
    memcpy(header + 1, &size, 8);

    uint64_t packets = packetNum;
    memcpy(header + 9, &packets, 8);

    memcpy(header + 17, shaSum, 32);

    // Write header
    size_t written = 0;
    while (written < sizeof(header)) {
        ssize_t res = write(serialfd, header, sizeof(header));
        if (res == -1) {
            perror("write");
            return 1;
        }
        written += res;
    }

    for (size_t i = 0; i < packetNum;) {
        char pktpath[PATH_MAX];
        strcpy(pktpath, pktdir);
        sprintf(pktpath + strlen(pktpath), "/%lu.pkt", i);

        // Debug info
        printf("%s\n", pktpath);

        FILE *pktp = fopen(pktpath, "r");
        if (pktp == NULL) {
            perror("fopen");
            return 1;
        }

        if(fseek(pktp, 0, SEEK_END) == -1) {
            perror("fseek");
            return 1;
        }

        long len = ftell(pktp);
        rewind(pktp);

        if (len > UINT16_MAX) {
            printf("length of packet file %s is too long.\n", pktpath);
            return 1;
        }

        // 1 byte for packet command
        // 2 bytes for packet length
        // 4 bytes for crc32sum
        uint8_t *pktData = malloc(7 + len);

        pktData[0] = TRANSFER_PACKET;

        uint16_t pktLen = len;
        memcpy(pktData + 1, &pktLen, 2);

        size_t amtRead = fread(pktData + 7, 1, len, pktp);
        if (amtRead != len) {
            printf("Error reading packet file %s.\n", pktpath);
            return 1;
        }
        fclose(pktp);

        uint32_t crc = crc32(pktData + 7, pktLen);
        memcpy(pktData + 3, &crc, 4);

        // write the packet to the serial fd
        size_t pktWritten = 0;
        while (pktWritten < len + 7) {
            ssize_t res = write(serialfd, pktData, len + 7);
            if (res == -1) {
                perror("write");
                return 1;
            }
            pktWritten += res;
        }
        free(pktData);

        // read the response from the other side
        uint8_t response = 0;
        ssize_t res = read(serialfd, &response, 1);
        if (res == -1) {
            perror("read");
            return 1;
        }
        if (response == 0) {
            printf("Recieved null response after sending packet %ld.\n", i);
            return 1;
        }

        // parse the response
        if (response == TRANSFER_NEXT)
            i += 1;
        else if (response == TRANSFER_AGAIN)
            printf("Recieved transfer again response.\n");
        else if (response == TRANSFER_END) {
            printf("Recieved a transfer end response.\n");
            return 1;
        } else if (response == TRANSFER_ERROR) {
            printf("Recieved transfer error response.\n");
            return 1;
        } else {
            printf("Recieved erroneous response.\n");
            return 1;
        }
    }

    // write the transfer end message
    uint8_t transEnd = TRANSFER_END;
    ssize_t res = write(serialfd, &transEnd, 1);
    if (res == -1) {
        perror("write");
        return 1;
    }

    close(serialfd);
}
