#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "protocol.h"
#include "rx_frontend.h"

#define TAG "FSO_RX"

#define UART_PORT UART_NUM_0
#define UART_BUF 256

#define RX_DIGITAL_PIN 4
#define RX_ADC_CH ADC1_CHANNEL_6  // GPIO34

#define DEFAULT_BITRATE 5000
#define MIN_BITRATE 1000
#define MAX_BITRATE 10000

typedef enum {
    ST_SEARCH_PREAMBLE = 0,
    ST_SYNC1,
    ST_SYNC2,
    ST_HEADER,
    ST_PAYLOAD,
    ST_CRC,
} parser_state_t;

typedef struct {
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
} parser_t;

static volatile int g_bitrate = DEFAULT_BITRATE;
static volatile bool g_use_analog = false;

static rx_frontend_t g_frontend;

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

static void emit_rx_line(const parser_t *p, uint8_t seq, uint8_t len, bool crc_ok, const uint8_t *payload, int rssi, const int *eye, int eye_len)
{
    uint32_t ber_ppm = 0;
    if (p->total_frames > 0) {
        ber_ppm = (uint32_t)((p->crc_fail * 1000000ULL) / p->total_frames);
    }

    printf("RX seq=%u len=%u crc=%d ber_ppm=%lu rssi=%d hex=", seq, len, crc_ok ? 1 : 0, (unsigned long)ber_ppm, rssi);
    for (uint8_t i = 0; i < len; i++) {
        printf("%02X", payload[i]);
    }

    printf(" eye=");
    for (int i = 0; i < eye_len; i++) {
        printf("%d", eye[i]);
        if (i + 1 < eye_len) {
            printf(",");
        }
    }
    printf("\n");
}

static void parser_push_byte(parser_t *p, uint8_t b, int rssi, const int *eye, int eye_len)
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

            emit_rx_line(p, p->header[0], p->header[2], ok, p->payload, rssi, eye, eye_len);

            p->state = ST_SEARCH_PREAMBLE;
            p->preamble_count = 0;
        }
        break;
    }
}

static void rx_task(void *arg)
{
    (void)arg;
    parser_t parser;
    parser_init(&parser);

    uint8_t cur_byte = 0;
    int bit_count = 0;

    int half_acc[2] = {0, 0};
    int half_i = 0;

    int eye[64] = {0};
    int eye_i = 0;

    while (1) {
        int analog = 0;
        int digi = 0;
        int lvl = rx_frontend_read_level(&g_frontend, &analog, &digi);

        half_acc[half_i] = lvl;
        eye[eye_i++ % 64] = analog;
        half_i++;

        if (half_i == 2) {
            int a = half_acc[0];
            int b = half_acc[1];
            half_i = 0;

            int bit = 0;
            if (a == 0 && b == 1) {
                bit = 0;
            } else if (a == 1 && b == 0) {
                bit = 1;
            } else {
                bit = 0;
            }

            cur_byte = (uint8_t)((cur_byte << 1) | (bit & 1));
            bit_count++;
            if (bit_count == 8) {
                int rssi = analog;
                parser_push_byte(&parser, cur_byte, rssi, eye, 64);
                cur_byte = 0;
                bit_count = 0;
            }
        }

        int half_us = 500000 / g_bitrate;
        if (half_us < 40) {
            half_us = 40;
        }
        esp_rom_delay_us((uint32_t)half_us);
    }
}

static void command_task(void *arg)
{
    (void)arg;
    uint8_t buf[UART_BUF];
    char line[UART_BUF];
    int idx = 0;

    while (1) {
        int n = uart_read_bytes(UART_PORT, buf, sizeof(buf), pdMS_TO_TICKS(20));
        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\n' || c == '\r') {
                line[idx] = '\0';
                idx = 0;

                if (strncmp(line, "BITRATE ", 8) == 0) {
                    int br = atoi(line + 8);
                    if (br >= MIN_BITRATE && br <= MAX_BITRATE) {
                        g_bitrate = br;
                        printf("OK BITRATE %d\n", g_bitrate);
                    } else {
                        printf("ERR bitrate range %d..%d\n", MIN_BITRATE, MAX_BITRATE);
                    }
                } else if (strcmp(line, "MODE ANALOG") == 0) {
                    g_use_analog = true;
                    rx_frontend_init(&g_frontend, g_use_analog, RX_DIGITAL_PIN, RX_ADC_CH);
                    printf("OK MODE ANALOG\n");
                } else if (strcmp(line, "MODE DIGITAL") == 0) {
                    g_use_analog = false;
                    rx_frontend_init(&g_frontend, g_use_analog, RX_DIGITAL_PIN, RX_ADC_CH);
                    printf("OK MODE DIGITAL\n");
                } else if (strlen(line) > 0) {
                    printf("ERR unknown cmd\n");
                }
            } else if (idx < (int)(sizeof(line) - 1)) {
                line[idx++] = c;
            }
        }
    }
}

void app_main(void)
{
    rx_frontend_init(&g_frontend, g_use_analog, RX_DIGITAL_PIN, RX_ADC_CH);
    printf("FSO_RX ready bitrate=%d mode=%s\n", g_bitrate, g_use_analog ? "ANALOG" : "DIGITAL");

    xTaskCreate(rx_task, "rx_task", 4096, NULL, 8, NULL);
    xTaskCreate(command_task, "command_task", 4096, NULL, 6, NULL);
}
