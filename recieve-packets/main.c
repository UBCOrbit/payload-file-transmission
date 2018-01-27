#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <linux/limits.h>

#include <crc32.h>
#include <protocol.h>

#define STORE_DIR "../packet-test2"

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("%s expects an input pipe and an output pipe.\n", argv[0]);
        return 1;
    }

    int infd = open(argv[1], O_RDONLY);
    if (infd == -1) {
        perror("open");
        return 1;
    }

    int outfd = open(argv[2], O_WRONLY);
    if (outfd == -1) {
        perror("open");
        return 1;
    }

    uint8_t command = 0;
    ssize_t res = read(infd, &command, 1);
    if (res == -1) {
        perror("read");
        return 1;
    }

    // If the command isn't transfer start, the command is invalid
    if (command != TRANSFER_START) {
        printf("Recieved erroneous message instead of transfer_start.\n");
        return 1;
    }

    // read the transfer start header
    uint8_t header[48];
    size_t amtRead = 0;
    while (amtRead < 48) {
        ssize_t hRes = read(infd, header + amtRead, 48 - amtRead);
        if (hRes == -1) {
            perror("read");
            return 1;
        }
        amtRead += hRes;
    }

    size_t fileSize  = *((uint64_t *) (header + 0));
    size_t packetNum = *((uint64_t *) (header + 8));

    uint8_t shaSum[32];
    memcpy(shaSum, header + 16, 32);

    char shaStr[65];
    for (size_t i = 0; i < 32; ++i)
        sprintf(shaStr + 2*i, "%02x", shaSum[i]);

    shaStr[64] = '\0';

    // Debug info
    printf("%s\n", shaStr);

    char pktDir[PATH_MAX] = STORE_DIR;
    strcat(pktDir, "/");
    strcat(pktDir, shaStr);

    // make sure the dir doesn't exist
    // TODO: transfer resume functionality
    int dirRes = mkdir(pktDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (dirRes == -1) {
        perror("mkdir");
        return 1;
    }

    // Create metadata file
    char metaPath[PATH_MAX];
    strcpy(metaPath, pktDir);
    strcat(metaPath, ".meta");

    FILE *metafp = fopen(metaPath, "w");
    if (metafp == NULL) {
        perror("fopen");
        return 1;
    }

    fprintf(metafp, "%lu\n%lu\n", fileSize, packetNum);
    fclose(metafp);

    // Start reading packets
    for (size_t i = 0; i < packetNum;) {
        // Debug info
        printf("recieving packet %lu.\n", i);

        uint8_t pktCommand = 0;
        ssize_t pktRes = read(infd, &pktCommand, 1);
        if (pktRes == -1) {
            perror("read");
            return 1;
        }

        if (pktCommand == TRANSFER_END) {
            printf("Recieved a premature transfer_end command.\n");
            return 1;
        } else if (pktCommand != TRANSFER_PACKET) {
            printf("Recieved erroneous command instead of transfer_packet.\n");
            return 1;
        }

        // read packet header
        uint8_t pktHeader[6];
        size_t pktAmtRead = 0;
        while (pktAmtRead < 6) {
            pktRes = read(infd, pktHeader, 6 - pktAmtRead);
            if (pktRes == -1) {
                perror("read");
                return 1;
            }
            pktAmtRead += pktRes;
        }

        uint16_t packetLen = *((uint16_t *) (pktHeader + 0));
        uint32_t crcSum    = *((uint32_t *) (pktHeader + 2));

        uint8_t *pktData = malloc(packetLen);

        // read packet data
        pktAmtRead = 0;
        while (pktAmtRead < packetLen) {
            pktRes = read(infd, pktData, packetLen - pktAmtRead);
            if (pktRes == -1) {
                perror("read");
                return 1;
            }
            pktAmtRead += pktRes;
        }

        // calculate the crc32sum on this end to verify packet integrity
        uint32_t crcSumCalc = crc32(pktData, packetLen);

        // if packet is intact, increment i and send TRANSFER_NEXT
        // otherwise send TRANSFER_AGAIN
        uint8_t pktSendCmd = TRANSFER_NEXT;
        if (crcSum != crcSumCalc) {
            printf("Error recieving packet: calculated crc32sum %u differs from given %u.\n",
                   crcSumCalc, crcSum);
            pktSendCmd = TRANSFER_AGAIN;
        }

        pktRes = write(outfd, &pktSendCmd, 1);
        if (pktRes == -1) {
            perror("write");
            return 1;
        }

        // if the packet wasn't intact, then it shouldn't be written
        // this is fugly af, but idc
        if (pktSendCmd == TRANSFER_AGAIN) {
            free(pktData);
            continue;
        }

        // write the packet data to the specified file
        char pktpath[PATH_MAX];
        strcpy(pktpath, pktDir);
        sprintf(pktpath + strlen(pktpath), "/%lu.pkt", i);

        FILE *pktp = fopen(pktpath, "w");
        if (pktp == NULL) {
            perror("fopen");
            printf("Error opening packet file for write: %s.\n", pktpath);
            return 1;
        }

        // Debug info
        printf("Packet: %lu, %u\n", i, packetLen);

        size_t written = fwrite(pktData, 1, packetLen, pktp);
        if (written != packetLen) {
            printf("Error writing data to pktet file: %s.\n", pktpath);
            return 1;
        }

        fclose(pktp);
        free(pktData);

        i += 1;
    }

    // read the transfer_end command
    res = read(infd, &command, 1);
    if (res == -1) {
        perror("read");
        return 1;
    }

    if (command != TRANSFER_END) {
        printf("Recieved erroneous command instead of transfer_end.\n");
        return 1;
    }

    close(infd);
    close(outfd);
}
