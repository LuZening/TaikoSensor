/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_composite_desc.h
  * @brief          : Header for usbd_composite_desc.c file.
  *                    Composite device descriptors (CDC + HID)
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
#ifndef __USBD_COMPOSITE_DESC__H__
#define __USBD_COMPOSITE_DESC__H__

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_def.h"

/* USER CODE BEGIN INCLUDE */

/* USER CODE END INCLUDE */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief For Usb device.
  * @{
  */

/** @defgroup USBD_COMPOSITE_DESC USBD_COMPOSITE_DESC
  * @brief Usb composite device descriptors module.
  * @{
  */

/** @defgroup USBD_COMPOSITE_DESC_Exported_Constants USBD_COMPOSITE_DESC_Exported_Constants
  * @brief Constants.
  * @{
  */
#define         DEVICE_ID1          (UID_BASE)
#define         DEVICE_ID2          (UID_BASE + 0x4)
#define         DEVICE_ID3          (UID_BASE + 0x8)
#define HID_IN_EP									0x81U /* EP1 in for data OUT */
#define CDC_IN_EP                                   0x82U  /* EP2 in for data IN */
#define CDC_OUT_EP                                  0x01U  /* EP1 out for data OUT */
#define CDC_CMD_EP                                  0x83U  /* EP3 in for CDC commands */
#define  USB_SIZ_STRING_SERIAL       0x1A

/* USER CODE BEGIN EXPORTED_CONSTANTS */
#define COMPOSITE_CDC_CONTROL_INTERFACE    1U
#define COMPOSITE_CDC_DATA_INTERFACE       2U
#define COMPOSITE_HID_INTERFACE            0U



#define USBD_LANGID_STRING_DRUMCONTROLLER     1033
#define USBD_MANUFACTURER_STRING_DRUMCONTROLLER     "5233 Lab"

#define USBD_PRODUCT_STRING_FS_DRUMCONTROLLER     "Taiko Sensor"
#define USBD_CONFIGURATION_STRING_FS_DRUMCONTROLLER     "HID Config"
#define USBD_INTERFACE_STRING_FS_DRUMCONTROLLER     "HID Interface"
/* USER CODE END EXPORTED_CONSTANTS */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_DESC_Exported_Defines USBD_COMPOSITE_DESC_Exported_Defines
  * @brief Defines.
  * @{
  */

/* User may redefined these in USER CODE section */

#define USBD_INTERFACE_STRING_CDC_CONTROL_FS     "CDC Control"
#define USBD_INTERFACE_STRING_CDC_DATA_FS        "CDC Data"
#define USBD_INTERFACE_STRING_HID_FS            "HID Keyboard"

/* Descriptor sizes */
#define USB_COMPOSITE_CONFIG_DESC_SIZ    108U


/* CDC descriptor sizes (from CDC template) */
#define USB_CDC_CONFIG_DESC_SIZ          67U

/* IAD sizes */
#define USB_IAD_DESC_SIZ                 8U

/* IAD for CDC Functional Descriptor Type */
#define IAD_DESCRIPTOR_TYPE              0x0BU

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_DESC_Exported_TypesDefinitions USBD_COMPOSITE_DESC_Exported_TypesDefinitions
  * @brief Types.
  * @{
  */

/* USER CODE BEGIN EXPORTED_TYPES */

/* USER CODE END EXPORTED_TYPES */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_DESC_Exported_Macros USBD_COMPOSITE_DESC_Exported_Macros
  * @brief Aliases.
  * @{
  */

/* USER CODE BEGIN EXPORTED_MACRO */

/* USER CODE END EXPORTED_MACRO */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_DESC_Exported_Variables USBD_COMPOSITE_DESC_Exported_Variables
  * @brief Public variables.
  * @{
  */

/** Descriptor for the Usb device. */
extern USBD_DescriptorsTypeDef FS_Composite_Desc;

/** Full Speed Configuration Descriptor */
extern uint8_t USBD_Composite_FS_CfgDesc[USB_COMPOSITE_CONFIG_DESC_SIZ];

/* USER CODE BEGIN EXPORTED_VARIABLES */
#define DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE 0
#define DEVICE_TYPE_DRUMCONTROLLER 1
extern uint8_t g_USB_device_type;
/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_DESC_Exported_FunctionsPrototype USBD_COMPOSITE_DESC_Exported_FunctionsPrototype
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

#endif /* __USBD_COMPOSITE_DESC__H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
