#include "uart_comm.h"
#include "config.h"
#include "fsm.h"
#include "settings.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart1;

static float live_v = 0.0f;
static float live_a = 0.0f;
static uint8_t reset_flag = 0;

#define RX_BUF 64
static char    rx_buf[RX_BUF];
static uint8_t rx_idx = 0;

/* Interrupt-fed ring buffer: bytes are not lost while main loop is busy
   (status TX, ADC polling). */
#define RING_SIZE 128
static volatile uint8_t  ring[RING_SIZE];
static volatile uint16_t ring_head = 0;
static volatile uint16_t ring_tail = 0;

void uart_comm_rx_isr(void)
{
    uint32_t sr = USART1->SR;
    if (sr & (USART_SR_RXNE | USART_SR_ORE)) {
        uint8_t d = (uint8_t)USART1->DR;       /* read clears RXNE/ORE */
        uint16_t next = (uint16_t)((ring_head + 1) % RING_SIZE);
        if (next != ring_tail) { ring[ring_head] = d; ring_head = next; }
    }
}

static void uart_print(const char *s) {
    HAL_UART_Transmit(&huart1, (const uint8_t *)s, (uint16_t)strlen(s), 200);
}

static void process_cmd(char *cmd)
{
    char reply[160];
    if (strncmp(cmd, "SET_OV:", 7) == 0) {
        if (settings_set_ov((float)atof(cmd + 7)))
            snprintf(reply, sizeof(reply), "[OK] OV=%.0fV\r\n", settings_get_ov());
        else
            snprintf(reply, sizeof(reply), "[ERR] OV must be > UV and <= %.0fV\r\n", MAX_VOLT_SETTING);
    } else if (strncmp(cmd, "SET_UV:", 7) == 0) {
        if (settings_set_uv((float)atof(cmd + 7)))
            snprintf(reply, sizeof(reply), "[OK] UV=%.0fV\r\n", settings_get_uv());
        else
            snprintf(reply, sizeof(reply), "[ERR] UV must be > 0 and < OV\r\n");
    } else if (strncmp(cmd, "SET_OC:", 7) == 0) {
        if (settings_set_oc((float)atof(cmd + 7)))
            snprintf(reply, sizeof(reply), "[OK] OC=%.1fA\r\n", settings_get_oc());
        else
            snprintf(reply, sizeof(reply), "[ERR] OC must be > 0 and <= %.0fA\r\n", MAX_CURR_SETTING);
    } else if (strcmp(cmd, "RESET") == 0) {
        reset_flag = 1;
        snprintf(reply, sizeof(reply), "[OK] Reset queued\r\n");
    } else if (strcmp(cmd, "STATUS") == 0) {
        snprintf(reply, sizeof(reply),
            "[STATUS] VOLT:%.1fV CURR:%.2fA STATE:%s FAULT:%s OV:%.0fV UV:%.0fV OC:%.1fA\r\n",
            live_v, live_a, fsm_state_str(), fsm_fault_str(),
            settings_get_ov(), settings_get_uv(), settings_get_oc());
    } else if (strcmp(cmd, "HELP") == 0) {
        uart_print("Commands:\r\n"
                   "  SET_OV:<V>  overvoltage  (e.g. SET_OV:230)\r\n"
                   "  SET_UV:<V>  undervoltage (e.g. SET_UV:180)\r\n"
                   "  SET_OC:<A>  overcurrent  (e.g. SET_OC:8)\r\n"
                   "  RESET       manual reset after fault\r\n"
                   "  STATUS      print status now\r\n"
                   "  HELP        this list\r\n");
        return;
    } else {
        snprintf(reply, sizeof(reply), "[ERR] Unknown: %s\r\n", cmd);
    }
    uart_print(reply);
}

void uart_comm_init(void) {
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    uart_print("\r\n=== Voltage Protector v1.0 ===\r\nType HELP for commands\r\n");
}

void uart_comm_process(void) {
    while (ring_tail != ring_head) {
        uint8_t ch = ring[ring_tail];
        ring_tail = (uint16_t)((ring_tail + 1) % RING_SIZE);
        if (ch == '\r' || ch == '\n') {
            if (rx_idx > 0) {
                rx_buf[rx_idx] = '\0';
                process_cmd(rx_buf);
                rx_idx = 0;
            }
        } else if (rx_idx < RX_BUF - 1) {
            rx_buf[rx_idx++] = (char)ch;
        }
    }
}

void    uart_comm_update_readings(float v, float a) { live_v = v; live_a = a; }
uint8_t uart_reset_requested(void) { return reset_flag; }
void    uart_clear_reset(void)     { reset_flag = 0;    }
