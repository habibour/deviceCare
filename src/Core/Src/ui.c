#include "ui.h"
#include "config.h"
#include "settings.h"
#include "fsm.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

/* ---- bit-banged I2C master (write only) on PB6/PB7 ---- */
#define SCL_H() (BTN_PORT->BSRR = LCD_SCL_PIN)
#define SCL_L() (BTN_PORT->BRR  = LCD_SCL_PIN)
#define SDA_H() (BTN_PORT->BSRR = LCD_SDA_PIN)
#define SDA_L() (BTN_PORT->BRR  = LCD_SDA_PIN)
/* PB7 mode bits live in CRL[31:28]: 0x3 = out PP 50 MHz, 0x4 = input floating */
#define SDA_OUT() (GPIOB->CRL = (GPIOB->CRL & ~(0xFu << 28)) | (0x3u << 28))
#define SDA_IN()  (GPIOB->CRL = (GPIOB->CRL & ~(0xFu << 28)) | (0x4u << 28))

static void i2c_dly(void) { for (volatile int i = 0; i < 20; i++) {} }

static void i2c_start(void) { SDA_H(); SCL_H(); i2c_dly(); SDA_L(); i2c_dly(); SCL_L(); i2c_dly(); }
static void i2c_stop(void)  { SDA_L(); SCL_H(); i2c_dly(); SDA_H(); i2c_dly(); }

static void i2c_write(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        if (b & (1u << i)) SDA_H(); else SDA_L();
        i2c_dly(); SCL_H(); i2c_dly(); SCL_L(); i2c_dly();
    }
    SDA_IN();                       /* ACK slot: release the line, ignore result */
    SCL_H(); i2c_dly(); SCL_L();
    SDA_OUT(); i2c_dly();
}

static void pcf_write(uint8_t d)
{
    i2c_start();
    i2c_write((uint8_t)(LCD_I2C_ADDR << 1));
    i2c_write(d);
    i2c_stop();
}

/* ---- HD44780 16x2 via PCF8574 backpack (P0=RS P2=E P3=BL P4..7=D4..D7) ---- */
#define LCD_RS 0x01
#define LCD_EN 0x04
#define LCD_BL 0x08

static void lcd_nibble(uint8_t nib, uint8_t rs)
{
    uint8_t d = (uint8_t)((nib << 4) | LCD_BL | rs);
    pcf_write(d | LCD_EN);
    pcf_write(d);
}

static void lcd_send(uint8_t v, uint8_t rs)
{
    lcd_nibble(v >> 4, rs);
    lcd_nibble(v & 0x0F, rs);
}

static void lcd_init(void)
{
    HAL_Delay(50);
    lcd_nibble(0x3, 0); HAL_Delay(5);
    lcd_nibble(0x3, 0); HAL_Delay(1);
    lcd_nibble(0x3, 0);
    lcd_nibble(0x2, 0);             /* 4-bit mode */
    lcd_send(0x28, 0);              /* 2 lines, 5x8 */
    lcd_send(0x0C, 0);              /* display on, cursor off */
    lcd_send(0x06, 0);              /* entry mode */
    lcd_send(0x01, 0); HAL_Delay(2);/* clear */
}

static void lcd_line(uint8_t row, const char *text)
{
    lcd_send((uint8_t)(0x80 | (row ? 0x40 : 0x00)), 0);
    for (int i = 0; i < 16; i++)
        lcd_send(text[i] ? (uint8_t)text[i] : ' ', LCD_RS);
}

/* ---- UI state ---- */
typedef enum { UI_VIEW = 0, UI_EDIT_OV, UI_EDIT_UV, UI_EDIT_OC } ui_mode_t;

static ui_mode_t mode = UI_VIEW;
static float live_v, live_a;
static uint8_t  prev_sel = 1, prev_up = 1, prev_dn = 1;
static uint32_t last_btn_tick, last_input_tick, last_draw_tick, msg_until;
static char     msg[17];
static char     shown[2][17];

void ui_init(void)
{
    GPIO_InitTypeDef g = {0};

    g.Pin   = LCD_SCL_PIN | LCD_SDA_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BTN_PORT, &g);
    SDA_H(); SCL_H();

    g.Pin   = BTN_SEL_PIN | BTN_UP_PIN | BTN_DOWN_PIN;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(BTN_PORT, &g);

    lcd_init();
    memset(shown, 0, sizeof(shown));
    last_btn_tick = last_input_tick = last_draw_tick = HAL_GetTick();
}

void ui_set_readings(float v, float a) { live_v = v; live_a = a; }

static void show_msg(const char *m)
{
    snprintf(msg, sizeof(msg), "%s", m);
    msg_until = HAL_GetTick() + 1500;
}

static void adjust(int dir)
{
    int ok = 1;
    switch (mode) {
    case UI_EDIT_OV: ok = settings_set_ov(settings_get_ov() + dir * VOLT_STEP); break;
    case UI_EDIT_UV: ok = settings_set_uv(settings_get_uv() + dir * VOLT_STEP); break;
    case UI_EDIT_OC: ok = settings_set_oc(settings_get_oc() + dir * CURR_STEP); break;
    default: break;
    }
    if (!ok) show_msg("LIMIT REACHED");
}

static const char *fault_short(void)
{
    switch (fsm_get_fault()) {
    case FAULT_OVERVOLTAGE:  return "OV";
    case FAULT_UNDERVOLTAGE: return "UV";
    case FAULT_OVERCURRENT:  return "OC";
    default:                 return "--";
    }
}

static void draw(void)
{
    char l1[24], l2[24];

    if (mode == UI_VIEW) {
        snprintf(l1, sizeof(l1), "%5.1fV %5.2fA", live_v, live_a);
        switch (fsm_get_state()) {
        case STATE_MONITORING:   snprintf(l2, sizeof(l2), "OK  relay ON");   break;
        case STATE_DISCONNECTED: snprintf(l2, sizeof(l2), "TRIP %s relayOFF", fault_short()); break;
        default:                 snprintf(l2, sizeof(l2), "COOLDOWN...");    break;
        }
    } else {
        const char *name = (mode == UI_EDIT_OV) ? "OVERVOLTAGE"
                         : (mode == UI_EDIT_UV) ? "UNDERVOLTAGE" : "OVERCURRENT";
        snprintf(l1, sizeof(l1), "SET %s", name);
        if (mode == UI_EDIT_OC)
            snprintf(l2, sizeof(l2), "%4.1fA UP/DN/SEL", settings_get_oc());
        else
            snprintf(l2, sizeof(l2), "%4.0fV UP/DN/SEL",
                     mode == UI_EDIT_OV ? settings_get_ov() : settings_get_uv());
    }
    if (HAL_GetTick() < msg_until) snprintf(l2, sizeof(l2), "%s", msg);

    l1[16] = '\0'; l2[16] = '\0';
    if (strcmp(l1, shown[0])) { lcd_line(0, l1); strcpy(shown[0], l1); }
    if (strcmp(l2, shown[1])) { lcd_line(1, l2); strcpy(shown[1], l2); }
}

void ui_poll(void)
{
    uint32_t now = HAL_GetTick();

    /* sample buttons every 20 ms (doubles as debounce), act on falling edge */
    if ((now - last_btn_tick) >= 20) {
        last_btn_tick = now;
        uint8_t sel = HAL_GPIO_ReadPin(BTN_PORT, BTN_SEL_PIN);
        uint8_t up  = HAL_GPIO_ReadPin(BTN_PORT, BTN_UP_PIN);
        uint8_t dn  = HAL_GPIO_ReadPin(BTN_PORT, BTN_DOWN_PIN);

        if (!sel && prev_sel) {
            mode = (mode == UI_EDIT_OC) ? UI_VIEW : (ui_mode_t)(mode + 1);
            last_input_tick = now;
        }
        if (!up && prev_up && mode != UI_VIEW)  { adjust(+1); last_input_tick = now; }
        if (!dn && prev_dn && mode != UI_VIEW)  { adjust(-1); last_input_tick = now; }
        prev_sel = sel; prev_up = up; prev_dn = dn;
    }

    if (mode != UI_VIEW && (now - last_input_tick) >= UI_IDLE_TIMEOUT_MS)
        mode = UI_VIEW;

    if ((now - last_draw_tick) >= 200) {
        last_draw_tick = now;
        draw();
    }
}
