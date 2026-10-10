#include "relay.h"
#include "config.h"
#include "stm32f1xx_hal.h"

static uint8_t connected = 0;

void relay_init(void)       { relay_disconnect(); }
void relay_connect(void)    { HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_SET);   connected = 1; }
void relay_disconnect(void) { HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET); connected = 0; }
uint8_t relay_is_connected(void) { return connected; }
