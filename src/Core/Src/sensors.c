#include "sensors.h"
#include "config.h"
#include "stm32f1xx_hal.h"

extern ADC_HandleTypeDef hadc1;

/* One conversion. Returns 1 and fills *out on success, 0 on timeout/error. */
static int adc_convert(uint32_t channel, uint32_t *out)
{
    ADC_ChannelConfTypeDef cfg = {0};
    int ok = 0;
    cfg.Channel      = channel;
    cfg.Rank         = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &cfg) == HAL_OK &&
        HAL_ADC_Start(&hadc1) == HAL_OK) {
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            *out = HAL_ADC_GetValue(&hadc1);
            ok = 1;
        }
    }
    HAL_ADC_Stop(&hadc1);
    return ok;
}

/* Median of 3 valid conversions: a single bad read (e.g. 0 after a timeout)
   cannot hide a real fault or fake one. Falls back to the last good value. */
static uint32_t adc_read_channel(uint32_t channel, uint32_t *last_good)
{
    uint32_t s[3];
    int n = 0;
    for (int tries = 0; tries < 6 && n < 3; tries++) {
        uint32_t v;
        if (adc_convert(channel, &v)) s[n++] = v;
    }
    if (n < 3) return *last_good;   /* ADC kept failing: hold previous reading */

    if (s[0] > s[1]) { uint32_t t = s[0]; s[0] = s[1]; s[1] = t; }
    if (s[1] > s[2]) { uint32_t t = s[1]; s[1] = s[2]; s[2] = t; }
    if (s[0] > s[1]) { uint32_t t = s[0]; s[0] = s[1]; s[1] = t; }
    *last_good = s[1];
    return s[1];
}

static uint32_t last_volt_raw, last_curr_raw;

void  sensors_init(void) {}

float sensors_read_voltage(void)
{
    return (float)adc_read_channel(VOLT_ADC_CHANNEL, &last_volt_raw) * VOLT_SCALE;
}

float sensors_read_current(void)
{
    return (float)adc_read_channel(CURR_ADC_CHANNEL, &last_curr_raw) * CURR_SCALE;
}
