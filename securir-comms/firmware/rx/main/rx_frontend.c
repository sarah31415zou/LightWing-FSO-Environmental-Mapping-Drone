#include "rx_frontend.h"

#include "driver/adc.h"
#include "driver/gpio.h"

void rx_frontend_init(rx_frontend_t *ctx, bool use_analog, int digital_pin, int adc_channel)
{
    if (!ctx) {
        return;
    }
    ctx->use_analog = use_analog;
    ctx->digital_pin = digital_pin;
    ctx->adc_channel = adc_channel;
    ctx->baseline = 0.0f;
    ctx->alpha = 0.01f;
    ctx->threshold = 1000.0f;
    ctx->samples = 0;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << digital_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    if (use_analog) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten((adc1_channel_t)adc_channel, ADC_ATTEN_DB_11);
    }
}

int rx_frontend_read_level(rx_frontend_t *ctx, int *analog_value, int *digital_level)
{
    if (!ctx) {
        return 0;
    }

    int digi = gpio_get_level((gpio_num_t)ctx->digital_pin);
    if (digital_level) {
        *digital_level = digi;
    }

    int analog = 0;
    if (ctx->use_analog) {
        analog = adc1_get_raw((adc1_channel_t)ctx->adc_channel);
        if (ctx->samples == 0) {
            ctx->baseline = (float)analog;
        } else {
            ctx->baseline = (1.0f - ctx->alpha) * ctx->baseline + ctx->alpha * (float)analog;
        }
        ctx->threshold = ctx->baseline + 80.0f;
        if (analog_value) {
            *analog_value = analog;
        }
        ctx->samples++;
        return analog > (int)ctx->threshold ? 1 : 0;
    }

    if (analog_value) {
        *analog_value = digi ? 4095 : 0;
    }
    return digi;
}
