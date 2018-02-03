#include "sha256_utils.h"

#include <stdint.h>
#include <stdio.h>

#include "sha256.h"

void calculateSHA256(const void *data, size_t len, uint8_t shaSum[32])
{
    SHA256_CTX shaCtx;

    sha256_init(&shaCtx);
    sha256_update(&shaCtx, data, len);
    sha256_final(&shaCtx, (BYTE *) shaSum);
}

void sha256Str(char shaStr[65], const uint8_t shaSum[32])
{
    for (size_t i = 0; i < 32; ++i)
        sprintf(shaStr + 2 * i, "%02x", shaSum[i]);
    shaStr[64] = '\0';
}

#ifdef SHA256_TEST

#include <stdlib.h>

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

int main(int argc, char **argv)
{
    size_t len;
    uint8_t *data;
    uint8_t shaSum[32];
    char shaStr[65];

    if (argc < 2) {
        printf("%s expects a file to check the sha256 sum of.\n", argv[0]);
        exit(-1);
    }

    data = readFile(argv[1], &len);

    calculateSHA256(data, len, shaSum);
    sha256Str(shaStr, shaSum);

    printf("%s\n", shaStr);

    free(data);
}

#endif // SHA256_TEST
