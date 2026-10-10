#include "settings.h"
#include "config.h"

static float ov = DEFAULT_OV;
static float uv = DEFAULT_UV;
static float oc = DEFAULT_OC;

float settings_get_ov(void) { return ov; }
float settings_get_uv(void) { return uv; }
float settings_get_oc(void) { return oc; }

int settings_set_ov(float v)
{
    if (v <= uv || v > MAX_VOLT_SETTING) return 0;
    ov = v;
    return 1;
}

int settings_set_uv(float v)
{
    if (v <= 0.0f || v >= ov) return 0;
    uv = v;
    return 1;
}

int settings_set_oc(float v)
{
    if (v <= 0.0f || v > MAX_CURR_SETTING) return 0;
    oc = v;
    return 1;
}
