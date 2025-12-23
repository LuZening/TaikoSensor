/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    : usbd_composite_wrapper.h
  * @brief   : Header for USB Composite Device Wrapper (CDC + HID)
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
#ifndef __USBD_COMPOSITE_WRAPPER_H
#define __USBD_COMPOSITE_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_def.h"
#include "usbd_cdc.h"
#include "usbd_hid.h"
#include "usbd_composite_desc.h"

/* USER CODE BEGIN INCLUDE */
extern volatile uint8_t g_HID_report_modified; // to indicate if HID report has been modified before been transmitted (dirty data)
/* USER CODE END INCLUDE */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief For Usb device.
  * @{
  */

/** @defgroup USBD_COMPOSITE_WRAPPER USBD_COMPOSITE_WRAPPER
  * @brief USB Composite Device Wrapper
  * @{
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Exported_Defines USBD_COMPOSITE_WRAPPER_Exported_Defines
  * @brief Defines.
  * @{
  */



/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Exported_TypesDefinitions USBD_COMPOSITE_WRAPPER_Exported_TypesDefinitions
  * @brief Types.
  * @{
  */

/* External reference to CDC Device Qualifier Descriptor */
extern uint8_t USBD_CDC_DeviceQualifierDesc[];

typedef struct
{
  USBD_CDC_HandleTypeDef hcdc;    /* CDC Class Data */
  USBD_HID_HandleTypeDef hhid;    /* HID Class Data */
  uint8_t CDC_ClassId;            /* CDC Class Instance ID */
  uint8_t HID_ClassId;            /* HID Class Instance ID */
} USBD_Composite_HandleTypeDef;

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Exported_Macros USBD_COMPOSITE_WRAPPER_Exported_Macros
  * @brief Aliases.
  * @{
  */

/* USER CODE BEGIN EXPORTED_MACRO */

/* USER CODE END EXPORTED_MACRO */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Exported_Variables USBD_COMPOSITE_WRAPPER_Exported_Variables
  * @brief Public variables.
  * @{
  */

/** Composite class structure */
extern USBD_ClassTypeDef USBD_COMPOSITE;

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Exported_FunctionsPrototype USBD_COMPOSITE_WRAPPER_Exported_FunctionsPrototype
  * @brief Public functions declaration.
  * @{
  */

/* USER CODE BEGIN EXPORTED_FUNCTIONS */

/* USER CODE END EXPORTED_FUNCTIONS */

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

#endif /* __USBD_COMPOSITE_WRAPPER_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
