#include "sha256_utils.h"

#include <stdint.h>
#include <stdio.h>

#include "sha256.h"

void calculateSHA256(const void *data, size_t len, uint8_t shaSum[32])
{
    SHA256_CTX shaCtx;

    sha256_init(&shaCtx);
    sha256_update(&shaCtx, data, len);
    sha256_final(&shaCtx, (BYTE *) &shaSum);
}

void sha256Str(char shaStr[65], const uint8_t shaSum[32])
{
    for (size_t i = 0; i < 32; ++i)
        sprintf(shaStr + 2 * i, "%02x", shaSum[i]);
    shaStr[64] = '\0';
}

