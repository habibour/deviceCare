/* USER CODE BEGIN Header */
/**
  * @file    main.c
  * @brief   Voltage Protector — OV / UV / OC protection, STM32F103C8T6
  */
/* USER CODE END Header */
#include "main.h"
/* USER CODE BEGIN Includes */
#include "config.h"
#include "sensors.h"
#include "relay.h"
#include "fsm.h"
#include "uart_comm.h"
#include "settings.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef  hadc1;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static uint32_t last_sample_tick = 0;
static uint32_t last_status_tick = 0;
static uint32_t last_hb_tick     = 0;
static uint8_t  hb_state         = 0;
static float    last_v = 0.0f, last_a = 0.0f;
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_USART1_UART_Init();

    /* USER CODE BEGIN 2 */
    sensors_init();
    relay_init();
    fsm_init();
    uart_comm_init();
    ui_init();
    last_sample_tick = last_status_tick = last_hb_tick = HAL_GetTick();
    /* USER CODE END 2 */

    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* 1. UART command polling */
        uart_comm_process();
        ui_poll();

        /* 2. Sample sensors every 100 ms */
        if ((now - last_sample_tick) >= SAMPLE_INTERVAL_MS)
        {
            last_sample_tick = now;
            float voltage = sensors_read_voltage();
            float current = sensors_read_current();
            uart_comm_update_readings(voltage, current);
            ui_set_readings(voltage, current);
            last_v = voltage; last_a = current;

            uint8_t reset_req = uart_reset_requested();
            if (reset_req) uart_clear_reset();
            if (HAL_GPIO_ReadPin(RESET_BTN_PORT, RESET_BTN_PIN) == GPIO_PIN_RESET)
                reset_req = 1;

            fsm_update(voltage, current,
                       settings_get_ov(), settings_get_uv(), settings_get_oc(),
                       reset_req);
        }

        /* 3. Print status every 1 s */
        if ((now - last_status_tick) >= STATUS_INTERVAL_MS)
        {
            last_status_tick = now;
            float v = last_v;
            float a = last_a;
            char line[180];
            snprintf(line, sizeof(line),
                "[STATUS] VOLT:%.1fV CURR:%.2fA STATE:%s FAULT:%s "
                "OV:%.0fV UV:%.0fV OC:%.1fA\r\n",
                v, a, fsm_state_str(), fsm_fault_str(),
                settings_get_ov(), settings_get_uv(), settings_get_oc());
            HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)strlen(line), 200);
        }

        /* 4. Heartbeat blink */
        {
            uint32_t hb_period = (fsm_get_fault() != FAULT_NONE)
                                 ? HEARTBEAT_FAULT_MS : HEARTBEAT_NORMAL_MS;
            if ((now - last_hb_tick) >= hb_period) {
                last_hb_tick = now;
                hb_state ^= 1;
                HAL_GPIO_WritePin(HEARTBEAT_PORT, HEARTBEAT_PIN,
                                  hb_state ? GPIO_PIN_RESET : GPIO_PIN_SET);
            }
        }
    }
}

/* System clock: HSI/2 x16 = 64 MHz (no external crystal needed for Wokwi) */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType       = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState             = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue  = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState         = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource        = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL           = RCC_PLL_MUL16;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PB0 — Relay LED output */
    HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin   = RELAY_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RELAY_PORT, &GPIO_InitStruct);

    /* PC13 — Heartbeat LED (active-LOW) */
    HAL_GPIO_WritePin(HEARTBEAT_PORT, HEARTBEAT_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin  = HEARTBEAT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(HEARTBEAT_PORT, &GPIO_InitStruct);

    /* PB12 — Reset button (INPUT_PULLUP) */
    GPIO_InitStruct.Pin  = RESET_BTN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(RESET_BTN_PORT, &GPIO_InitStruct);
}

static void MX_ADC1_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    hadc1.Instance                   = ADC1;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
    HAL_ADCEx_Calibration_Start(&hadc1);
}

static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_9;           /* TX */
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_10;           /* RX */
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
