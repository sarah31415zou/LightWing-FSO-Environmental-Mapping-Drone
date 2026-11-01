#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"

static const int RX_DIGITAL_PIN = 14;

static const int DEFAULT_BITRATE = 2000;
static const int MIN_BITRATE = 500;
static const int MAX_BITRATE = 10000;

enum parser_state_t {
    ST_SEARCH_PREAMBLE = 0,
    ST_SYNC1,
    ST_SYNC2,
    ST_HEADER,
    ST_PAYLOAD,
    ST_CRC,
};

struct parser_t {
    parser_state_t state;
    uint8_t preamble_count;
    uint8_t header[3];
    uint8_t payload[FSO_MAX_PAYLOAD];
    uint8_t header_idx;
    uint8_t payload_idx;
    uint8_t crc_bytes[2];
    uint8_t crc_idx;
    uint32_t total_frames;
    uint32_t crc_fail;
    uint32_t good_frames;
};

struct rx_frontend_t {
    uint32_t samples;
    uint32_t detected_samples;
};

static parser_t g_parser = {};
static rx_frontend_t g_frontend = {};
static int g_bitrate = DEFAULT_BITRATE;

static char g_line[256];
static int g_line_idx = 0;

static uint8_t g_cur_byte = 0;
static int g_bit_count = 0;
static int g_half_acc[2] = {0, 0};
static int g_half_i = 0;
static int g_eye[64] = {0};
static int g_eye_i = 0;
static uint32_t g_next_sample_us = 0;

static void parser_reset_frame(parser_t *p)
{
    p->header_idx = 0;
    p->payload_idx = 0;
    p->crc_idx = 0;
}

static void parser_init(parser_t *p)
{
    memset(p, 0, sizeof(*p));
    p->state = ST_SEARCH_PREAMBLE;
}

static void rx_frontend_init(rx_frontend_t *ctx)
{
    ctx->samples = 0;
    ctx->detected_samples = 0;
    pinMode(RX_DIGITAL_PIN, INPUT_PULLUP);
}

static int rx_frontend_read_level()
{
    // VS1838B output is active LOW when 38kHz carrier is detected.
    // Convert to logical-high = optical mark present.
    int raw = digitalRead(RX_DIGITAL_PIN);
    return raw == LOW ? 1 : 0;
}

static void emit_rx_line(const parser_t *p, uint8_t seq, uint8_t len, bool crc_ok, const uint8_t *payload, int rssi)
{
    uint32_t ber_ppm = 0;
    if (p->total_frames > 0) {
        ber_ppm = (uint32_t)((p->crc_fail * 1000000ULL) / p->total_frames);
    }

    Serial.printf("RX seq=%u len=%u crc=%d ber_ppm=%lu rssi=%d hex=", seq, len, crc_ok ? 1 : 0, (unsigned long)ber_ppm, rssi);
    for (uint8_t i = 0; i < len; i++) {
        Serial.printf("%02X", payload[i]);
    }

    Serial.print(" eye=");
    for (int i = 0; i < 64; i++) {
        int idx = (g_eye_i + i) % 64;
        Serial.print(g_eye[idx]);
        if (i < 63) Serial.print(',');
    }
    Serial.print('\n');
}

static void parser_push_byte(parser_t *p, uint8_t b, int rssi)
{
    switch (p->state) {
        case ST_SEARCH_PREAMBLE:
            if (b == FSO_PREAMBLE_BYTE) {
                p->preamble_count++;
                if (p->preamble_count >= FSO_PREAMBLE_LEN) {
                    p->state = ST_SYNC1;
                }
            } else {
                p->preamble_count = 0;
            }
            break;

        case ST_SYNC1:
            if (b == FSO_SYNC_WORD_HI) {
                p->state = ST_SYNC2;
            } else {
                p->state = ST_SEARCH_PREAMBLE;
                p->preamble_count = 0;
            }
            break;

        case ST_SYNC2:
            if (b == FSO_SYNC_WORD_LO) {
                parser_reset_frame(p);
                p->state = ST_HEADER;
            } else {
                p->state = ST_SEARCH_PREAMBLE;
                p->preamble_count = 0;
            }
            break;

        case ST_HEADER:
            p->header[p->header_idx++] = b;
            if (p->header_idx == 3) {
                if (p->header[2] > FSO_MAX_PAYLOAD) {
                    p->state = ST_SEARCH_PREAMBLE;
                    p->preamble_count = 0;
                } else if (p->header[2] == 0) {
                    p->state = ST_CRC;
                } else {
                    p->state = ST_PAYLOAD;
                }
            }
            break;

        case ST_PAYLOAD:
            p->payload[p->payload_idx++] = b;
            if (p->payload_idx >= p->header[2]) {
                p->state = ST_CRC;
            }
            break;

        case ST_CRC:
            p->crc_bytes[p->crc_idx++] = b;
            if (p->crc_idx == 2) {
                uint8_t work[3 + FSO_MAX_PAYLOAD];
                work[0] = p->header[0];
                work[1] = p->header[1];
                work[2] = p->header[2];
                memcpy(&work[3], p->payload, p->header[2]);

                uint16_t calc = fso_crc16_ccitt(work, (uint16_t)(3 + p->header[2]));
                uint16_t got = (uint16_t)((p->crc_bytes[0] << 8) | p->crc_bytes[1]);

                p->total_frames++;
                bool ok = calc == got;
                if (ok) {
                    p->good_frames++;
                } else {
                    p->crc_fail++;
                }

                emit_rx_line(p, p->header[0], p->header[2], ok, p->payload, rssi);

                p->state = ST_SEARCH_PREAMBLE;
                p->preamble_count = 0;
            }
            break;
    }
}

static void sample_once()
{
    int lvl = rx_frontend_read_level();
    g_frontend.samples++;
    g_frontend.detected_samples += (lvl ? 1u : 0u);

    g_half_acc[g_half_i] = lvl;
    g_eye[g_eye_i++ % 64] = lvl ? 4095 : 0;  // Simulate "eye" with digital levels (0 or max)
    g_half_i++;

    if (g_half_i == 2) {
        int a = g_half_acc[0];
        int b = g_half_acc[1];
        g_half_i = 0;

        int bit = 0;
        if (a == 0 && b == 1) {
            bit = 0;
        } else if (a == 1 && b == 0) {
            bit = 1;
        }

        g_cur_byte = (uint8_t)((g_cur_byte << 1) | (bit & 1));
        g_bit_count++;
        if (g_bit_count == 8) {
            int rssi_proxy = 0;
            if (g_frontend.samples > 0) {
                rssi_proxy = (int)((g_frontend.detected_samples * 4095UL) / g_frontend.samples);
            }
            parser_push_byte(&g_parser, g_cur_byte, rssi_proxy);
            g_cur_byte = 0;
            g_bit_count = 0;
        }
    }
}

static void process_command(const char *line)
{
    if (strncmp(line, "BITRATE ", 8) == 0) {
        int br = atoi(line + 8);
        if (br >= MIN_BITRATE && br <= MAX_BITRATE) {
            g_bitrate = br;
            Serial.printf("OK BITRATE %d\n", g_bitrate);
        } else {
            Serial.printf("ERR bitrate range %d..%d\n", MIN_BITRATE, MAX_BITRATE);
        }
        return;
    }

    if (strcmp(line, "MODE DIGITAL") == 0) {
        Serial.println("OK MODE DIGITAL");
        return;
    }

    if (strcmp(line, "MODE ANALOG") == 0) {
        Serial.println("ERR MODE ANALOG unsupported on VS1838B");
        return;
    }

    if (line[0] != '\0') {
        Serial.println("ERR unknown cmd");
    }
}

static void poll_serial_commands()
{
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (g_line_idx > 0) {
                g_line[g_line_idx] = '\0';
                process_command(g_line);
                g_line_idx = 0;
            }
        } else if (g_line_idx < (int)sizeof(g_line) - 1) {
            g_line[g_line_idx++] = c;
        }
    }
}


void setup()
{
    Serial.begin(115200);
    delay(200);

    parser_init(&g_parser);
    rx_frontend_init(&g_frontend);

    int half_us = 500000 / g_bitrate;
    if (half_us < 40) half_us = 40;
    g_next_sample_us = micros() + (uint32_t)half_us;

    Serial.printf("FSO_RX ready bitrate=%d PIN=%d (VS1838B active-low)\n", g_bitrate, RX_DIGITAL_PIN);
}

void loop()
{
    poll_serial_commands();

    // Debug: Print level changes always (idle or not)
    static int last = -1;
    int cur = digitalRead(RX_DIGITAL_PIN);
    if (cur != last) {
      Serial.printf("Pin 14 raw level: %d (inverted to %d)\n", cur, rx_frontend_read_level());
      last = cur;
    }

    int half_us = 500000 / g_bitrate;
    if (half_us < 40) half_us = 40;

    uint32_t now = micros();
    int iterations = 0;
    while ((int32_t)(now - g_next_sample_us) >= 0 && iterations < 100) {
        sample_once();
        g_next_sample_us += (uint32_t)half_us;
        now = micros();
        yield();  // Prevent blocking
        iterations++;
    }
}