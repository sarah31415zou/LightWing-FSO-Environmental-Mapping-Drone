#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"

static const int TX_PIN = 18;
static const int MIN_BITRATE = 500;
static const int MAX_BITRATE = 10000;
static const int DEFAULT_BITRATE = 2000;
static const int CARRIER_HALF_PERIOD_US = 13;

static int g_bitrate = DEFAULT_BITRATE;
static bool g_pattern_enable = false;
static uint8_t g_seq = 0;
static uint32_t g_next_pattern_ms = 0;
static uint8_t g_pattern_counter = 0;

static char g_line[256];
static int g_line_idx = 0;

static void tx_mark_carrier_us(unsigned int duration_us)
{
    uint32_t start = micros();
    while ((uint32_t)(micros() - start) < duration_us) {
        digitalWrite(TX_PIN, HIGH);
        delayMicroseconds(CARRIER_HALF_PERIOD_US);
        digitalWrite(TX_PIN, LOW);
        delayMicroseconds(CARRIER_HALF_PERIOD_US);
    }
}

static void tx_halfbit(int level)
{
    int half_us = 500000 / g_bitrate;
    if (half_us < 40) {
        half_us = 40;
    }
    if (level) {
        tx_mark_carrier_us((unsigned int)half_us);
    } else {
        digitalWrite(TX_PIN, LOW);
        delayMicroseconds((unsigned int)half_us);
    }
}

static void tx_manchester_bit(int bit)
{
    if (bit == 0) {
        tx_halfbit(LOW);
        tx_halfbit(HIGH);
    } else {
        tx_halfbit(HIGH);
        tx_halfbit(LOW);
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
    digitalWrite(TX_PIN, LOW);
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
        out[n++] = (uint8_t)strtoul(tmp, nullptr, 16);
        hex += 2;
        while (*hex == ' ') {
            hex++;
        }
    }
    return n;
}

static void send_payload(const uint8_t *payload, uint8_t length)
{
    fso_frame_t frame = {};
    frame.seq = g_seq++;
    frame.flags = 0;
    frame.length = length;
    memcpy(frame.payload, payload, length);

    uint8_t raw[FSO_PREAMBLE_LEN + 2 + 3 + FSO_MAX_PAYLOAD + 2] = {0};
    uint16_t raw_len = fso_build_raw_frame(&frame, raw, sizeof(raw));
    if (raw_len == 0) {
        Serial.println("ERR frame build");
        return;
    }

    tx_send_raw_bytes(raw, raw_len);
    Serial.printf("TX_DONE seq=%u len=%u bitrate=%d\n", frame.seq, frame.length, g_bitrate);
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

    if (strncmp(line, "SEND ", 5) == 0) {
        uint8_t payload[FSO_MAX_PAYLOAD] = {0};
        size_t len = hex_to_bytes(line + 5, payload, sizeof(payload));
        if (len == 0) {
            Serial.println("ERR SEND bad hex");
        } else {
            send_payload(payload, (uint8_t)len);
        }
        return;
    }

    if (strcmp(line, "PATTERN START") == 0) {
        g_pattern_enable = true;
        g_next_pattern_ms = millis();
        Serial.println("OK PATTERN START");
        return;
    }

    if (strcmp(line, "PATTERN STOP") == 0) {
        g_pattern_enable = false;
        Serial.println("OK PATTERN STOP");
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

static void poll_pattern()
{
    if (!g_pattern_enable) {
        return;
    }

    uint32_t now = millis();
    if ((int32_t)(now - g_next_pattern_ms) < 0) {
        return;
    }

    uint8_t payload[16];
    for (uint8_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)(g_pattern_counter + i);
    }
    send_payload(payload, sizeof(payload));
    g_pattern_counter++;
    g_next_pattern_ms = now + 20;
}

void setup()
{
    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, LOW);

    Serial.begin(115200);
    delay(200);
    Serial.printf("FSO_TX ready bitrate=%d carrier=38kHz\n", g_bitrate);
}

void loop()
{
    poll_serial_commands();
    poll_pattern();
}
