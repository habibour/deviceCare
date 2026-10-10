#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

#define VOLT_ADC_CHANNEL   ADC_CHANNEL_0
#define CURR_ADC_CHANNEL   ADC_CHANNEL_1

#define RELAY_PIN          GPIO_PIN_0
#define RELAY_PORT         GPIOB
#define HEARTBEAT_PIN      GPIO_PIN_13
#define HEARTBEAT_PORT     GPIOC
#define RESET_BTN_PIN      GPIO_PIN_12
#define RESET_BTN_PORT     GPIOB

/* On-device UI: 3 buttons (active-low, pull-up) + I2C 16x2 LCD (PCF8574, bit-banged) */
#define BTN_PORT           GPIOB
#define BTN_SEL_PIN        GPIO_PIN_13   /* cycle OV -> UV -> OC -> view */
#define BTN_UP_PIN         GPIO_PIN_14
#define BTN_DOWN_PIN       GPIO_PIN_15
#define LCD_SCL_PIN        GPIO_PIN_6    /* PB6 */
#define LCD_SDA_PIN        GPIO_PIN_7    /* PB7 */
#define LCD_I2C_ADDR       0x27
#define UI_IDLE_TIMEOUT_MS 10000
#define VOLT_STEP          5.0f
#define CURR_STEP          0.5f
#define MAX_VOLT_SETTING   330.0f        /* sensor full scale */
#define MAX_CURR_SETTING   20.0f

#define VOLT_SCALE         (330.0f / 4095.0f)
#define CURR_SCALE         (20.0f  / 4095.0f)

#define DEFAULT_OV         250.0f
#define DEFAULT_UV         170.0f
#define DEFAULT_OC         10.0f

#define SAMPLE_INTERVAL_MS    100
#define STATUS_INTERVAL_MS   1000
#define COOLDOWN_MS          3000
#define HEARTBEAT_NORMAL_MS  1000
#define HEARTBEAT_FAULT_MS    200

#endif /* INC_CONFIG_H_ */
