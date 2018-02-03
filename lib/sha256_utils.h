#ifndef sha256_utils_h_INCLUDED
#define sha256_utils_h_INCLUDED

#include <stddef.h>
#include <stdint.h>

void calculateSHA256(const void *data, size_t len, uint8_t shaSum[32]);
void sha256Str(char shaStr[65], const uint8_t shaSum[32]);

#endif // sha256_utils_h_INCLUDED

