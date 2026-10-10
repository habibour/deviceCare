#include "fsm.h"
#include "config.h"
#include "relay.h"
#include "stm32f1xx_hal.h"

static FSM_State   state      = STATE_MONITORING;
static Fault_Cause fault      = FAULT_NONE;
static uint32_t    state_tick = 0;

void fsm_init(void)
{
    state = STATE_MONITORING;
    fault = FAULT_NONE;
    state_tick = HAL_GetTick();
    relay_connect();
}

void fsm_update(float voltage, float current,
                float ov, float uv, float oc,
                uint8_t reset_requested)
{
    uint32_t now = HAL_GetTick();

    switch (state) {
    case STATE_MONITORING:
        if      (voltage > ov) { fault = FAULT_OVERVOLTAGE;  }
        else if (voltage < uv) { fault = FAULT_UNDERVOLTAGE; }
        else if (current > oc) { fault = FAULT_OVERCURRENT;  }
        if (fault != FAULT_NONE) {
            relay_disconnect();
            state = STATE_DISCONNECTED;
            state_tick = now;
        }
        break;

    case STATE_DISCONNECTED: {
        uint8_t cleared =
            (fault == FAULT_OVERVOLTAGE  && voltage <= ov) ||
            (fault == FAULT_UNDERVOLTAGE && voltage >= uv) ||
            (fault == FAULT_OVERCURRENT  && current <= oc);
        if (cleared || reset_requested) {
            fault = FAULT_NONE;
            state = STATE_COOLDOWN;
            state_tick = now;
        }
        break;
    }

    case STATE_COOLDOWN:
        if ((now - state_tick) >= COOLDOWN_MS) {
            relay_connect();
            state = STATE_MONITORING;
            state_tick = now;
        }
        break;
    }
}

FSM_State   fsm_get_state(void) { return state; }
Fault_Cause fsm_get_fault(void) { return fault; }

const char *fsm_state_str(void) {
    switch (state) {
    case STATE_MONITORING:   return "MONITORING";
    case STATE_DISCONNECTED: return "DISCONNECTED";
    case STATE_COOLDOWN:     return "COOLDOWN";
    default:                 return "UNKNOWN";
    }
}

const char *fsm_fault_str(void) {
    switch (fault) {
    case FAULT_NONE:          return "NONE";
    case FAULT_OVERVOLTAGE:   return "OVERVOLTAGE";
    case FAULT_UNDERVOLTAGE:  return "UNDERVOLTAGE";
    case FAULT_OVERCURRENT:   return "OVERCURRENT";
    default:                  return "UNKNOWN";
    }
}
