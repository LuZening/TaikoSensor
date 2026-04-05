/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
#define N_ADC_CHANNELS 5
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TIMER_48K htim3
#define XLED1_Pin GPIO_PIN_12
#define XLED1_GPIO_Port GPIOB
#define XLED2_Pin GPIO_PIN_13
#define XLED2_GPIO_Port GPIOB
#define XLED3_Pin GPIO_PIN_14
#define XLED3_GPIO_Port GPIOB
#define XLED4_Pin GPIO_PIN_15
#define XLED4_GPIO_Port GPIOB
#define K1_Pin GPIO_PIN_15
#define K1_GPIO_Port GPIOA
#define K2_Pin GPIO_PIN_3
#define K2_GPIO_Port GPIOB
#define K3_Pin GPIO_PIN_4
#define K3_GPIO_Port GPIOB
#define XGOOD_Pin GPIO_PIN_5
#define XGOOD_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
extern uint8_t g_k1_pressed, g_k2_pressed;
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
