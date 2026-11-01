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

static inline uint16_t fso_build_raw_frame(const fso_frame_t *frame, uint8_t *out, uint16_t out_max)
{
    if (!frame || !out || frame->length > FSO_MAX_PAYLOAD) {
        return 0;
    }

    uint16_t need = (uint16_t)(FSO_PREAMBLE_LEN + 2 + 3 + frame->length + 2);
    if (out_max < need) {
        return 0;
    }

    uint16_t idx = 0;
    for (uint8_t i = 0; i < FSO_PREAMBLE_LEN; i++) {
        out[idx++] = FSO_PREAMBLE_BYTE;
    }

    out[idx++] = FSO_SYNC_WORD_HI;
    out[idx++] = FSO_SYNC_WORD_LO;

    out[idx++] = frame->seq;
    out[idx++] = frame->flags;
    out[idx++] = frame->length;

    for (uint8_t i = 0; i < frame->length; i++) {
        out[idx++] = frame->payload[i];
    }

    uint16_t crc = fso_crc16_ccitt(&out[FSO_PREAMBLE_LEN + 2], (uint16_t)(3 + frame->length));
    out[idx++] = (uint8_t)((crc >> 8) & 0xFF);
    out[idx++] = (uint8_t)(crc & 0xFF);
    return idx;
}
