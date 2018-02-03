#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include <linux/limits.h>

#include "sha256.h"

#define PACKET_DIR "received-packets"

int main(int argc, char **argv)
{
    /*
    // TODO: proper argument parsing
    if (argc < 3) {
        printf("%s expects a sha256sum id and a path to store the stitched file as arguments.\n", argv[0]);
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

    // Debug info
    printf("%lu, %lu\n", fileSize, packetNum);

    uint8_t *data = malloc(fileSize);
    size_t offset = 0;

    for (size_t i = 0; i < packetNum; ++i) {
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

        size_t read = fread(data + offset, 1, len, pktp);
        if (read != len) {
            printf("Error reading packet file %s.\n", pktpath);
            return 1;
        }
        fclose(pktp);

        offset += read;
    }

    if (offset != fileSize) {
        printf("Error reading packet files.\n");
        return 1;
    }

    FILE *outp = fopen(argv[2], "w");
    if (outp == NULL) {
        perror("fopen");
        return 1;
    }

    size_t written = fwrite(data, 1, fileSize, outp);
    if (written != fileSize) {
        printf("Error writing to output file %s.\n", argv[2]);
        return 1;
    }

    // Calculate sha256sum of stitched file
    SHA256_CTX shaCtx;
    uint8_t shaSum2[32];

    sha256_init(&shaCtx);
    sha256_update(&shaCtx, data, fileSize);
    sha256_final(&shaCtx, (BYTE *) shaSum2);

    for (size_t i = 0; i < 32; ++i) {
        if (shaSum[i] != shaSum2[i]) {
            printf("Sha256 sum of stitched file differs from given sum.\n");
            return 1;
        }
    }

    free(data);
    */
}
