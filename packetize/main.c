#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/limits.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "sha256.h"

// 2^15 bytes, 32 kb
#define PACKET_SIZE 0x8000

#define STORE_DIR "../packet-test"

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("%s expects a file to pktetize.\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("fopen");
        printf("Unable to open %s\n", argv[1]);
        exit(1);
    }

    if (fseek(fp, 0, SEEK_END) == -1) {
        perror("fseek");
        return 1;
    }

    long len = ftell(fp);
    rewind(fp);

    uint8_t *data = malloc(len);
    size_t read = fread(data, 1, len, fp);
    if (read != len) {
        printf("Error reading file %s.\n", argv[1]);
        return 1;
    }
    fclose(fp);

    // Calculate sha256 sum
    SHA256_CTX shaCtx;
    uint8_t shaSum[32];

    sha256_init(&shaCtx);
    sha256_update(&shaCtx, data, len);
    sha256_final(&shaCtx, (BYTE *) &shaSum);

    // Generate the string of the shasum for file name
    char shaStr[65];
    for (size_t i = 0; i < 32; ++i)
        snprintf(shaStr + 2 * i, sizeof(shaStr) - 2 * i, "%02x", shaSum[i]);
    shaStr[64] = 0;

    // Debug info
    printf("%s\n", shaStr);

    char dirpath[PATH_MAX] = STORE_DIR;
    //strcat(dirpath, STORE_DIR);
    strcat(dirpath, "/");
    strcat(dirpath, shaStr);

    // Debug info
    printf("%s\n", dirpath);

    // create a directory for the file pieces
    if (mkdir(dirpath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
        perror("mkdir");
        return 1;
    }

    // write file metadata
    //  file size
    //  number of pktets
    size_t numPackets = len / PACKET_SIZE + (len % PACKET_SIZE == 0 ? 0 : 1);

    // Debug info
    printf("%lu\n", numPackets);

    char metapath[PATH_MAX];
    strcpy(metapath, dirpath);
    strcat(metapath, ".meta");

    FILE *metap = fopen(metapath, "w");
    if (metap == NULL) {
        perror("fopen");
        printf("Error opening metadata file for write: %s.\n", metapath);
        return 1;
    }

    fprintf(metap, "%lu\n%lu\n", len, numPackets);
    if (ferror(metap)) {
        printf("Error writing to metadata file: %s.\n", metapath);
        return 1;
    }
    fclose(metap);

    // Split the file into packets
    for (size_t i = 0; i < numPackets; ++i) {
        char pktpath[PATH_MAX];
        strcpy(pktpath, dirpath);
        sprintf(pktpath + strlen(pktpath), "/%lu.pkt", i);

        FILE *pktp = fopen(pktpath, "w");
        if (pktp == NULL) {
            perror("fopen");
            printf("Error opening pktet file for write: %s.\n", pktpath);
            return 1;
        }

        size_t toWrite = len - i * PACKET_SIZE > PACKET_SIZE ? PACKET_SIZE : len - i * PACKET_SIZE;

        // Debug info
        printf("Packet: %lu, %lu\n", i, toWrite);

        size_t written = fwrite(data + i * PACKET_SIZE, 1, toWrite, pktp);
        if (written != toWrite) {
            printf("Error writing data to pktet file: %s.\n", pktpath);
            return 1;
        }

        fclose(pktp);
    }

    // and calculate the crc32 sum for each packet?

    free(data);
}
