#ifndef sha256_utils_h_INCLUDED
#define sha256_utils_h_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void sha256calc(const void *data, size_t len, uint8_t shaSum[32]);
void sha256str(char shaStr[65], const uint8_t shaSum[32]);
bool sha256cmp(const uint8_t shaSum1[32], const uint8_t shaSum2[32]);

#endif // sha256_utils_h_INCLUDED

