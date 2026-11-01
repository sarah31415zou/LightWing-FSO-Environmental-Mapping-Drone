#include "fec_hamming.h"

uint8_t hamming74_encode_nibble(uint8_t nibble)
{
    uint8_t d1 = (nibble >> 0) & 1;
    uint8_t d2 = (nibble >> 1) & 1;
    uint8_t d3 = (nibble >> 2) & 1;
    uint8_t d4 = (nibble >> 3) & 1;

    uint8_t p1 = d1 ^ d2 ^ d4;
    uint8_t p2 = d1 ^ d3 ^ d4;
    uint8_t p3 = d2 ^ d3 ^ d4;

    uint8_t cw = 0;
    cw |= (p1 << 0);
    cw |= (p2 << 1);
    cw |= (d1 << 2);
    cw |= (p3 << 3);
    cw |= (d2 << 4);
    cw |= (d3 << 5);
    cw |= (d4 << 6);
    return cw;
}

uint8_t hamming74_decode_codeword(uint8_t codeword, uint8_t *decoded_nibble, uint8_t *corrected)
{
    uint8_t b1 = (codeword >> 0) & 1;
    uint8_t b2 = (codeword >> 1) & 1;
    uint8_t b3 = (codeword >> 2) & 1;
    uint8_t b4 = (codeword >> 3) & 1;
    uint8_t b5 = (codeword >> 4) & 1;
    uint8_t b6 = (codeword >> 5) & 1;
    uint8_t b7 = (codeword >> 6) & 1;

    uint8_t s1 = b1 ^ b3 ^ b5 ^ b7;
    uint8_t s2 = b2 ^ b3 ^ b6 ^ b7;
    uint8_t s3 = b4 ^ b5 ^ b6 ^ b7;

    uint8_t syndrome = (uint8_t)(s1 | (s2 << 1) | (s3 << 2));
    if (corrected) {
        *corrected = 0;
    }

    if (syndrome >= 1 && syndrome <= 7) {
        codeword ^= (uint8_t)(1u << (syndrome - 1));
        if (corrected) {
            *corrected = 1;
        }
    }

    uint8_t d1 = (codeword >> 2) & 1;
    uint8_t d2 = (codeword >> 4) & 1;
    uint8_t d3 = (codeword >> 5) & 1;
    uint8_t d4 = (codeword >> 6) & 1;

    if (decoded_nibble) {
        *decoded_nibble = (uint8_t)(d1 | (d2 << 1) | (d3 << 2) | (d4 << 3));
    }

    return syndrome ? 1 : 0;
}
