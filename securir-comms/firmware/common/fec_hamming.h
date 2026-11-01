#pragma once

#include <stdint.h>

uint8_t hamming74_encode_nibble(uint8_t nibble);
uint8_t hamming74_decode_codeword(uint8_t codeword, uint8_t *decoded_nibble, uint8_t *corrected);
