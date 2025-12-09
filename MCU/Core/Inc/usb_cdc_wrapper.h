/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    : usb_cdc_wrapper.h
  * @brief   : Header for usb_cdc_wrapper.c file. Endpoint remapping wrapper
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

#ifndef __USB_CDC_WRAPPER_H__
#define __USB_CDC_WRAPPER_H__

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* Must include usbd_cdc.h before this file to override its endpoint defines */

/* USER CODE BEGIN INCLUDE */

#include "usbd_cdc.h"
/* USER CODE END INCLUDE */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief For Usb device.
  * @{
  */

/** @defgroup USBD_CDC_WRAPPER USBD_CDC_WRAPPER
  * @brief USB CDC Wrapper Module
  * @{
  */

/** @defgroup USBD_CDC_WRAPPER_Exported_Defines USBD_CDC_WRAPPER_Exported_Defines
  * @brief Defines.
  * @{
  */

/* Override CDC endpoint defines to avoid conflict with HID endpoint 0x81 */
/* HID already uses 0x81, so we remap CDC endpoints */


/* Packet sizes */
#define CDC_CMD_PACKET_SIZE                         8U  /* Control Endpoint Packet size */
#define CDC_DATA_FS_MAX_PACKET_SIZE                 64U  /* Endpoint IN & OUT Packet size */

/**
  * @}
  */

/** @defgroup USBD_CDC_WRAPPER_Exported_TypesDefinitions USBD_CDC_WRAPPER_Exported_TypesDefinitions
  * @brief Types.
  * @{
  */

/* USER CODE BEGIN EXPORTED_TYPES */

/* USER CODE END EXPORTED_TYPES */

/**
  * @}
  */

/** @defgroup USBD_CDC_WRAPPER_Exported_Macros USBD_CDC_WRAPPER_Exported_Macros
  * @brief Aliases.
  * @{
  */

/* USER CODE BEGIN EXPORTED_MACRO */

/* USER CODE END EXPORTED_MACRO */

/**
  * @}
  */

/** @defgroup USBD_CDC_WRAPPER_Exported_Variables USBD_CDC_WRAPPER_Exported_Variables
  * @brief Public variables.
  * @{
  */

/** CDC Interface callback. */
extern USBD_CDC_ItfTypeDef USBD_CDC_Interface_fops_FS;

/** CDC Wrapper class structure */
extern USBD_ClassTypeDef USBD_CDC_WRAPPER;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */


/* USER CODE BEGIN EXPORTED_FUNCTIONS */

/* USER CODE END EXPORTED_FUNCTIONS */

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

// call in main
void usb_cdc_process(void);
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __USB_CDC_WRAPPER_H__ */
