#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FSO_PREAMBLE_BYTE 0x55
#define FSO_PREAMBLE_LEN  8
#define FSO_SYNC_WORD_HI  0xD3
#define FSO_SYNC_WORD_LO  0x91
#define FSO_MAX_PAYLOAD   200

typedef struct {
    uint8_t seq;
    uint8_t flags;
    uint8_t length;
    uint8_t payload[FSO_MAX_PAYLOAD];
    uint16_t crc16;
} fso_frame_t;

static inline uint16_t fso_crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}
