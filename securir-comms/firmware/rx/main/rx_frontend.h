#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool use_analog;
    int digital_pin;
    int adc_channel;
    float baseline;
    float alpha;
    float threshold;
    uint32_t samples;
} rx_frontend_t;

void rx_frontend_init(rx_frontend_t *ctx, bool use_analog, int digital_pin, int adc_channel);
int rx_frontend_read_level(rx_frontend_t *ctx, int *analog_value, int *digital_level);
