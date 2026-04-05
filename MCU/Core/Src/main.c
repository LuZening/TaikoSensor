/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "piezo_config.h"
#include "piezosensor.h"
#include "keyboard.h"
#include "debug_output.h"
#include "sensor_config.h"
#include "uart_terminal.h"
#include "usb_cdc_wrapper.h"
#include "usbd_composite_desc.h"
#include "usbd_composite_wrapper.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart3_rx;

/* USER CODE BEGIN PV */
/* DMA_BUFFER_SIZE is defined in piezo_config.h as SAMPLES_PER_MS * SENSOR_TOTAL_CHANNELS * 2 */
static volatile __attribute__((aligned(4))) uint16_t arrADC1_DMA[DMA_BUFFER_SAMPLES] = {0};  // 480 samples for double buffering

/* XGOOD LED blinking state when config load fails */
typedef struct {
    uint32_t last_toggle_ms;  // Last time LED was toggled
    bool led_state;           // Current LED state (true = on)
} XGOOD_BlinkState_t;

static XGOOD_BlinkState_t g_xgood_blink = {0};

/* Key button state for sensor emulation - K1->sensor3, K2->sensor4, K3->ESC/repeat */
static uint8_t k1_prev = 1;
static uint32_t keys_last_ms = 0;
static uint8_t k2_prev = 1;
static uint8_t k3_prev = 1;
static uint8_t k3_repeat_active = 0;
static uint32_t k3_repeat_last_ms = 0;
#define K3_REPEAT_INTERVAL_MS 200
uint8_t g_k1_pressed = 0, g_k2_pressed = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/*================= LED CONTROL FUNCTIONS =================*/

/**
 * @brief Handle XGOOD LED blinking based on device state and USB mode
 * @param current_ms Current system time in milliseconds
 * @note Priority: config failure > drum controller mode > composite mode
 *       - Config failure: Blinks pattern: ON for 200ms, OFF for 800ms (1 second period)
 *       - Drum controller mode: Blink at 5Hz (200ms period, 100ms ON, 100ms OFF)
 *       - Composite mode: LED stays ON
 */
void xgood_led_blink_handler(uint32_t current_ms)
{
    if (sensor_config_load_failed()) {
        /* Config load failed - use failure blink pattern */
        uint32_t elapsed = current_ms - g_xgood_blink.last_toggle_ms;

        if (g_xgood_blink.led_state) {
            /* LED is currently ON - check if 200ms have passed */
            if (elapsed >= 200) {
                /* Turn LED OFF (active low, so set to 1) */
                HAL_GPIO_WritePin(XGOOD_GPIO_Port, XGOOD_Pin, GPIO_PIN_SET);
                g_xgood_blink.led_state = false;
                g_xgood_blink.last_toggle_ms = current_ms;
            }
        } else {
            /* LED is currently OFF - check if 800ms have passed */
            if (elapsed >= 800) {
                /* Turn LED ON (active low, so set to 0) */
                HAL_GPIO_WritePin(XGOOD_GPIO_Port, XGOOD_Pin, GPIO_PIN_RESET);
                g_xgood_blink.led_state = true;
                g_xgood_blink.last_toggle_ms = current_ms;
            }
        }
    }
    else if (g_USB_device_type == DEVICE_TYPE_DRUMCONTROLLER) {
        /* Drum controller mode - flash at 5Hz (200ms period, 100ms on, 100ms off) */
        uint32_t elapsed = current_ms - g_xgood_blink.last_toggle_ms;

        if (elapsed >= 100) { // 100ms = half period for 5Hz
            /* Toggle LED state */
            g_xgood_blink.led_state = !g_xgood_blink.led_state;
            HAL_GPIO_WritePin(XGOOD_GPIO_Port, XGOOD_Pin, g_xgood_blink.led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
            g_xgood_blink.last_toggle_ms = current_ms;
        }
    }
    else {
        /* Composite mode - LED stays ON */
        HAL_GPIO_WritePin(XGOOD_GPIO_Port, XGOOD_Pin, GPIO_PIN_RESET); // ON (active low)
    }
}

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Forward declarations for functions from piezosensor.c */
#include "piezosensor.h"

/* Include terminal processing in main loop */
void uart_terminal_tick(uint32_t current_ms);

/* Key button polling function - K1->sensor3, K2->sensor4, K3->ESC/repeat */
void key_button_poll(void);

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  HAL_Delay(10);

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_USART3_UART_Init();

  MX_TIM1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  // check USB device mode:
  // any of SW1 ~ SW3 LOW 	-> 	DEVICE_USB_DRUMCONTROLLER
  // otherwise				->	DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE

  uint8_t k1 = HAL_GPIO_ReadPin(K1_GPIO_Port, K1_Pin);
  uint8_t k2 = HAL_GPIO_ReadPin(K2_GPIO_Port, K2_Pin);
  uint8_t k3 = HAL_GPIO_ReadPin(K3_GPIO_Port, K3_Pin);
  if((k1 == 0) || (k2 == 0) || (k3 == 0))
	  g_USB_device_type = DEVICE_TYPE_DRUMCONTROLLER;
  else
	  g_USB_device_type = DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE;
  MX_USB_DEVICE_Init();
  /* Note: HAL_ADC_Start_DMA expects uint32_t*, but our buffer is uint16_t for efficiency
   * Casting to uint32_t* is safe and results in correct DMA transfer behavior */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)arrADC1_DMA, DMA_BUFFER_SAMPLES);
  HAL_TIM_Base_Start_IT(&htim3); // start TIM3 so ADC will start

  /* Initialize piezosensor FSM system */
  piezosensor_init();

  /* Initialize debug output */
  debug_init();

  /* Initialize sensor configuration (loads from flash or sets defaults) */
  sensor_config_init();

  /* Turn on XGOOD LED (active low) to indicate MCU is running */
  HAL_GPIO_WritePin(XGOOD_GPIO_Port, XGOOD_Pin, GPIO_PIN_RESET);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* Process UART terminal idle detection */
    uart_terminal_tick(g_system_ms);

    /* Process USB CDC RX and TX in main loop (async mechanism) */
    usb_cdc_process();

    /* Handle XGOOD LED blinking if config load failed */
    xgood_led_blink_handler(g_system_ms);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV2;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_USB;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV4;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 5;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 47;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 499;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 99;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 9;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, XLED1_Pin|XLED2_Pin|XLED3_Pin|XLED4_Pin
                          |XGOOD_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : XLED1_Pin XLED2_Pin XLED3_Pin XLED4_Pin
                           XGOOD_Pin */
  GPIO_InitStruct.Pin = XLED1_Pin|XLED2_Pin|XLED3_Pin|XLED4_Pin
                          |XGOOD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : K1_Pin */
  GPIO_InitStruct.Pin = K1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(K1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : K2_Pin K3_Pin */
  GPIO_InitStruct.Pin = K2_Pin|K3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

//static uint16_t arrADC1_DMA_copy[DMA_BUFFER_SAMPLES];
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        /* Process first half of DMA buffer (samples 0-239) */
//    	memcpy(arrADC1_DMA_copy, arrADC1_DMA, sizeof(arrADC1_DMA)); // for breakpoint
        extract_envelope_from_samples(arrADC1_DMA, 0);
        trigger_fsm_process();
        cooldown_fsm_process();
        decay_adjusted_threshold();
        increment_system_ms();

        /* Poll key buttons for sensor emulation */
        key_button_poll();

        /* Check if LEDs need to turn off */
        led_check_turn_off(g_system_ms);

        /* Output max values every 100ms */
        debug_output_if_needed(g_system_ms);

        /* Process afterglow expirations and send buffered HID reports */
        hid_process_afterglow();
        hid_send_buffered_reports();

        /* LED handling is done in main loop only to avoid interrupt context issues */
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        /* Process second half of DMA buffer (samples 240-479) */
//    	memcpy(arrADC1_DMA_copy, arrADC1_DMA, sizeof(arrADC1_DMA)); // for breakpoint
        extract_envelope_from_samples(arrADC1_DMA, 240);
        trigger_fsm_process();
        cooldown_fsm_process();
        decay_adjusted_threshold();
        increment_system_ms();
        /* Poll key buttons for sensor emulation */
        key_button_poll();

        /* Check if LEDs need to turn off */
        led_check_turn_off(g_system_ms);

        /* Output max values every 100ms */
        debug_output_if_needed(g_system_ms);

        /* Process afterglow expirations and send buffered HID reports */
        hid_process_afterglow();
        /* Poll key buttons for sensor emulation */
        hid_send_buffered_reports();

        /* LED handling is done in main loop only to avoid interrupt context issues */
    }
}

/**
 * @brief Key button polling function for sensor emulation
 *
 * K1 -> sensor 3 (left center) trigger
 * K2 -> sensor 4 (left rim) trigger
 * K3 -> ESC key (short click), or repeat sensor 3 (long press 500ms, repeat every 200ms)
 */
void key_button_poll(void)
{
	static uint8_t key_overwritten = 0;
    uint8_t k1 = HAL_GPIO_ReadPin(K1_GPIO_Port, K1_Pin);
    uint8_t k2 = HAL_GPIO_ReadPin(K2_GPIO_Port, K2_Pin);
    uint8_t k3 = HAL_GPIO_ReadPin(K3_GPIO_Port, K3_Pin);
    uint32_t now = g_system_ms;

    // K1 -> sensor 2 (right center) on press edge
    if (k1 == 0 && k1_prev == 1) {
    	g_k1_pressed = 1;
    	// overwrite the key code
    	g_sensors[1].drumcontroller_keycode = JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_Y; // X for calibrate
    	g_sensors[1].hid_keycode = KEY_J; // J for calibrate
    	schedule_key_press(1);
    	keys_last_ms = now;
    	key_overwritten = 1;
    }

    k1_prev = k1;

    // K2 -> sensor 4 (left rim) on press edge
    if (k2 == 0 && k2_prev == 1) {
    	g_k2_pressed = 1;
    	// overwrite the key code
    	g_sensors[1].drumcontroller_keycode = JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_X; // X for calibrate
    	g_sensors[1].hid_keycode = KEY_J; // J for calibrate
    	schedule_key_press(1);
    	keys_last_ms = now;
    	key_overwritten = 1;
    }
    k2_prev = k2;

    // K3 handling:
    // - Short click: ESC key
    // - Long press (hold 500ms): enter repeat mode, sends sensor 3 every 200ms
    // - Click again while repeating: exit repeat mode
    if (k3 == 0 && k3_prev == 1) {
        // K3 just pressed
        if (k3_repeat_active) {
            // Was in repeat mode - exit it
            k3_repeat_active = 0;
        } else {
            // Wasn't repeating - send ESC key (uses sensor 1's structure but with ESC keycode)
            g_sensors[2].hid_keycode = KEY_ESC;
            schedule_key_press(2);
            g_sensors[2].hid_keycode = SENSOR_1_KEYCODE;

            // Start long-press timer to enter repeat mode
            key_overwritten = 1;
            k3_repeat_last_ms = now;
        }
        keys_last_ms = now;
    }
    else if (k3 == 0 && !k3_repeat_active) {
        // K3 held - check for long press (500ms) to enter repeat mode
        if (now - k3_repeat_last_ms >= 500) {
            k3_repeat_active = 1;
            k3_repeat_last_ms = now;
        }
    }

    if (k3_repeat_active && k3 == 0) {
        // Repeat mode active - send sensor 3 every 200ms
        if (now - k3_repeat_last_ms >= K3_REPEAT_INTERVAL_MS) {
            schedule_key_press(2);  // sensor 3
            k3_repeat_last_ms = now;
        }
    }

    k3_prev = k3;

    // recover overwritten key codes
    if((now - keys_last_ms > 1000) &&  (key_overwritten))
    {
    	for(int isensor = 0; isensor < SENSORS_ACTIVE; ++isensor) init_keycodes(isensor);
    	key_overwritten = 0;
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
