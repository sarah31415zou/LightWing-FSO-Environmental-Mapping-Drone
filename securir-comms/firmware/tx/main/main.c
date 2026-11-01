#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "protocol.h"

#define TAG "FSO_TX"

#define TX_GPIO GPIO_NUM_18
#define UART_PORT UART_NUM_0
#define UART_BUF 256

#define MIN_BITRATE 1000
#define MAX_BITRATE 10000
#define DEFAULT_BITRATE 5000

static volatile int g_bitrate = DEFAULT_BITRATE;
static volatile bool g_pattern_enable = false;
static uint8_t g_seq = 0;

static void tx_halfbit(int level)
{
    gpio_set_level(TX_GPIO, level ? 1 : 0);
    int half_us = 500000 / g_bitrate;
    if (half_us < 40) {
        half_us = 40;
    }
    esp_rom_delay_us((uint32_t)half_us);
}

static void tx_manchester_bit(int bit)
{
    if (bit == 0) {
        tx_halfbit(0);
        tx_halfbit(1);
    } else {
        tx_halfbit(1);
        tx_halfbit(0);
    }
}

static void tx_send_raw_bytes(const uint8_t *raw, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t b = raw[i];
        for (int k = 7; k >= 0; k--) {
            tx_manchester_bit((b >> k) & 1);
        }
    }
    gpio_set_level(TX_GPIO, 0);
}

static size_t hex_to_bytes(const char *hex, uint8_t *out, size_t out_max)
{
    size_t n = 0;
    while (*hex && *(hex + 1) && n < out_max) {
        while (*hex == ' ') {
            hex++;
        }
        if (!isxdigit((unsigned char)hex[0]) || !isxdigit((unsigned char)hex[1])) {
            break;
        }
        char tmp[3] = {hex[0], hex[1], 0};
        out[n++] = (uint8_t)strtoul(tmp, NULL, 16);
        hex += 2;
        while (*hex == ' ') {
            hex++;
        }
    }
    return n;
}

static void send_payload(const uint8_t *payload, uint8_t length)
{
    fso_frame_t frame = {0};
    frame.seq = g_seq++;
    frame.flags = 0;
    frame.length = length;
    memcpy(frame.payload, payload, length);

    uint8_t raw[FSO_PREAMBLE_LEN + 2 + 3 + FSO_MAX_PAYLOAD + 2] = {0};
    uint16_t raw_len = fso_build_raw_frame(&frame, raw, sizeof(raw));
    if (raw_len == 0) {
        ESP_LOGW(TAG, "frame build failed");
        return;
    }

    tx_send_raw_bytes(raw, raw_len);
    printf("TX_DONE seq=%u len=%u bitrate=%d\n", frame.seq, frame.length, g_bitrate);
}

static void pattern_task(void *arg)
{
    (void)arg;
    uint8_t counter = 0;
    uint8_t payload[16];

    while (1) {
        if (g_pattern_enable) {
            for (uint8_t i = 0; i < sizeof(payload); i++) {
                payload[i] = (uint8_t)(counter + i);
            }
            send_payload(payload, sizeof(payload));
            counter++;
            vTaskDelay(pdMS_TO_TICKS(20));
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
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
                } else if (strncmp(line, "SEND ", 5) == 0) {
                    uint8_t payload[FSO_MAX_PAYLOAD] = {0};
                    size_t len = hex_to_bytes(line + 5, payload, sizeof(payload));
                    if (len == 0) {
                        printf("ERR SEND bad hex\n");
                    } else {
                        send_payload(payload, (uint8_t)len);
                    }
                } else if (strcmp(line, "PATTERN START") == 0) {
                    g_pattern_enable = true;
                    printf("OK PATTERN START\n");
                } else if (strcmp(line, "PATTERN STOP") == 0) {
                    g_pattern_enable = false;
                    printf("OK PATTERN STOP\n");
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
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << TX_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(TX_GPIO, 0);

    uart_set_rx_timeout(UART_PORT, 2);
    printf("FSO_TX ready bitrate=%d\n", g_bitrate);

    xTaskCreate(pattern_task, "pattern_task", 4096, NULL, 5, NULL);
    xTaskCreate(command_task, "command_task", 4096, NULL, 6, NULL);
}
