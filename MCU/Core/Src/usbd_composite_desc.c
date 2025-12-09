/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : Core/Src/usbd_composite_desc.c
  * @brief          : This file implements the USB composite device descriptors
  *                    (CDC + HID)
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
#include "usbd_core.h"
#include "usbd_composite_desc.h"
#include "usbd_conf.h"
#include "usbd_desc.h"

/* USER CODE BEGIN INCLUDE */
#include "usbd_hid.h"
/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
extern USBD_HandleTypeDef hUsbDeviceFS;
/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @{
  */

/** @addtogroup USBD_COMPOSITE_DESC
  * @{
  */

/** @defgroup USBD_COMPOSITE_DESC_Private_TypesDefinitions USBD_COMPOSITE_DESC_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_DESC_Private_Defines USBD_COMPOSITE_DESC_Private_Defines
  * @brief Private defines.
  * @{
  */

/* USER CODE BEGIN PRIVATE_DEFINES */

/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_DESC_Private_Macros USBD_COMPOSITE_DESC_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_DESC_Private_FunctionPrototypes USBD_COMPOSITE_DESC_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

static void Get_SerialNum(void);
static void IntTo_Unicode(uint32_t value, uint8_t *pbuf, uint8_t len);

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_DESC_Private_FunctionPrototypes USBD_COMPOSITE_DESC_Private_FunctionPrototypes
  * @brief Private functions declaration for FS.
  * @{
  */

uint8_t * USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_DESC_Private_Variables USBD_COMPOSITE_DESC_Private_Variables
  * @brief Private variables.
  * @{
  */

USBD_DescriptorsTypeDef FS_Composite_Desc =
{
  USBD_FS_DeviceDescriptor
, USBD_FS_LangIDStrDescriptor
, USBD_FS_ManufacturerStrDescriptor
, USBD_FS_ProductStrDescriptor
, USBD_FS_SerialStrDescriptor
, USBD_FS_ConfigStrDescriptor
, USBD_FS_InterfaceStrDescriptor
};

/**
  * @brief USB Standard Device Descriptor for composite device
  */
#if defined ( __ICCARM__ ) /* IAR Compiler */
  #pragma data_alignment=4
#endif /* defined ( __ICCARM__ ) */
__ALIGN_BEGIN uint8_t USBD_Composite_FS_DeviceDesc[USB_LEN_COMPOSITE_DEV_DESC] __ALIGN_END =
{
  0x12,                       /*bLength */
  USB_DESC_TYPE_DEVICE,       /*bDescriptorType*/
  0x00,                       /*bcdUSB */
  0x02,
  0xEF,                       /*bDeviceClass (0xEF = Miscellaneous Device Class)*/
  0x02,                       /*bDeviceSubClass (2 = Common Class)*/
  0x01,                       /*bDeviceProtocol (1 = Interface Association Descriptor)*/
  USB_MAX_EP0_SIZE,           /*bMaxPacketSize*/
  LOBYTE(USBD_VID),           /*idVendor*/
  HIBYTE(USBD_VID),           /*idVendor*/
  LOBYTE(USBD_PID_FS),        /*idProduct*/
  HIBYTE(USBD_PID_FS),        /*idProduct*/
  0x00,                       /*bcdDevice rel. 2.00*/
  0x02,
  USBD_IDX_MFC_STR,           /*Index of manufacturer string*/
  USBD_IDX_PRODUCT_STR,       /*Index of product string*/
  USBD_IDX_SERIAL_STR,        /*Index of serial number string*/
  USBD_MAX_NUM_CONFIGURATION  /*bNumConfigurations*/
};

/**
  * @brief USB Composite Configuration Descriptor
  * @details Combines CDC + HID with IADs
  * Total size: 108 bytes (67 CDC + 25 HID + 16 IADs)
  */
#if defined ( __ICCARM__ ) /* IAR Compiler */
  #pragma data_alignment=4
#endif /* defined ( __ICCARM__ ) */
__ALIGN_BEGIN uint8_t USBD_FS_CfgDesc[USB_COMPOSITE_CONFIG_DESC_SIZ] __ALIGN_END =
{
  /* Configuration Descriptor */
  0x09,                       /* bLength: Configuration Descriptor size */
  USB_DESC_TYPE_CONFIGURATION, /* bDescriptorType: Configuration */
  USB_COMPOSITE_CONFIG_DESC_SIZ, /* wTotalLength: Total size of data */
  0x00,
  0x03,                       /* bNumInterfaces: 3 interfaces (2 CDC (3 EPs) + 1 HID (1 EP)) */
  0x01,                       /* bConfigurationValue: Configuration value */
  0x00,                       /* iConfiguration: Index of string descriptor */
  0xC0,                       /* bmAttributes: self powered */
  0x32,                       /* MaxPower 100 mA */

  /*---------------------------------------------------------------------------*/
  /* IAD for CDC (associates CDC Control and Data interfaces) */
  0x08,                       /* bLength: IAD descriptor size */
  IAD_DESCRIPTOR_TYPE,        /* bDescriptorType: Interface Association Descriptor */
  0x00,                       /* bFirstInterface: First interface (CDC Control) */
  0x02,                       /* bInterfaceCount: Number of interfaces in this function */
  0x02,                       /* bFunctionClass: Communication Interface Class */
  0x02,                       /* bFunctionSubClass: Abstract Control Model */
  0x01,                       /* bFunctionProtocol: Common AT commands */
  0x00,                       /* iFunction: Index of string descriptor */

  /*---------------------------------------------------------------------------*/
  /* CDC Command Interface Descriptor */
  0x09,                       /* bLength: Interface Descriptor size */
  USB_DESC_TYPE_INTERFACE,    /* bDescriptorType: Interface */
  0x00,                       /* bInterfaceNumber: Number of Interface */
  0x00,                       /* bAlternateSetting: Alternate setting */
  0x01,                       /* bNumEndpoints: One endpoint used */
  0x02,                       /* bInterfaceClass: Communication Interface Class */
  0x02,                       /* bInterfaceSubClass: Abstract Control Model */
  0x01,                       /* bInterfaceProtocol: Common AT commands */
  0x00,                       /* iInterface: */

  /* Header Functional Descriptor */
  0x05,                       /* bLength: Endpoint Descriptor size */
  0x24,                       /* bDescriptorType: CS_INTERFACE */
  0x00,                       /* bDescriptorSubtype: Header Func Desc */
  0x10,                       /* bcdCDC: spec release number */
  0x01,

  /* Call Management Functional Descriptor */
  0x05,                       /* bFunctionLength */
  0x24,                       /* bDescriptorType: CS_INTERFACE */
  0x01,                       /* bDescriptorSubtype: Call Management Func Desc */
  0x00,                       /* bmCapabilities: D0+D1 */
  0x01,                       /* bDataInterface: 1 */

  /* ACM Functional Descriptor */
  0x04,                       /* bFunctionLength */
  0x24,                       /* bDescriptorType: CS_INTERFACE */
  0x02,                       /* bDescriptorSubtype: Abstract Control Management desc */
  0x02,                       /* bmCapabilities */

  /* Union Functional Descriptor */
  0x05,                       /* bFunctionLength */
  0x24,                       /* bDescriptorType: CS_INTERFACE */
  0x06,                       /* bDescriptorSubtype: Union func desc */
  0x00,                       /* bMasterInterface: Communication class interface */
  0x01,                       /* bSlaveInterface0: Data Class Interface */

  /* Endpoint 3 Descriptor - CDC Command (modified from 0x82 to avoid HID conflict) */
  0x07,                       /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,     /* bDescriptorType: Endpoint */
  0x83,                       /* bEndpointAddress - EP3 IN (0x83 = IN, endpoint 3) */
  0x03,                       /* bmAttributes: Interrupt */
  LOBYTE(0x0008),             /* wMaxPacketSize: 8 bytes */
  HIBYTE(0x0008),
  0x10,                       /* bInterval: 16ms */

  /*---------------------------------------------------------------------------*/
  /* CDC Data Interface */
  0x09,                       /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_INTERFACE,    /* bDescriptorType: Interface */
  0x01,                       /* bInterfaceNumber: Number of Interface */
  0x00,                       /* bAlternateSetting: Alternate setting */
  0x02,                       /* bNumEndpoints: Two endpoints used */
  0x0A,                       /* bInterfaceClass: CDC Data */
  0x00,                       /* bInterfaceSubClass: */
  0x00,                       /* bInterfaceProtocol: */
  0x00,                       /* iInterface: */

  /* Endpoint 1 OUT Descriptor - CDC Data OUT (unchanged) */
  0x07,                       /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,     /* bDescriptorType: Endpoint */
  0x01,                       /* bEndpointAddress - EP1 OUT (0x01 = OUT, endpoint 1) */
  0x02,                       /* bmAttributes: Bulk */
  LOBYTE(0x0040),             /* wMaxPacketSize: 64 bytes */
  HIBYTE(0x0040),
  0x00,                       /* bInterval: ignore for Bulk transfer */

  /* Endpoint 2 IN Descriptor - CDC Data IN (modified from 0x81 to avoid HID conflict) */
  0x07,                       /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,     /* bDescriptorType: Endpoint */
  0x82,                       /* bEndpointAddress - EP2 IN (0x82 = IN, endpoint 2) */
  0x02,                       /* bmAttributes: Bulk */
  LOBYTE(0x0040),             /* wMaxPacketSize: 64 bytes */
  HIBYTE(0x0040),
  0x00,                       /* bInterval: ignore for Bulk transfer */

  /*---------------------------------------------------------------------------*/
  /* IAD for HID (associates HID interface) */
  0x08,                       /* bLength: IAD descriptor size */
  IAD_DESCRIPTOR_TYPE,        /* bDescriptorType: Interface Association Descriptor */
  0x02,                       /* bFirstInterface: First interface (HID) */
  0x01,                       /* bInterfaceCount: Number of interfaces in this function */
  0x03,                       /* bFunctionClass: HID Class */
  0x01,                       /* bFunctionSubClass: 1 = Boot */
  0x01,                       /* bFunctionProtocol: 1 = Keyboard */
  0x00,                       /* iFunction: Index of string descriptor */

  /*---------------------------------------------------------------------------*/
  /* HID Interface Descriptor */
  0x09,         /*bLength: Interface Descriptor size*/
  USB_DESC_TYPE_INTERFACE,/*bDescriptorType: Interface descriptor type*/
  0x02,         /*bInterfaceNumber: Number of Interface*/
  0x00,         /*bAlternateSetting: Alternate setting*/
  0x01,         /*bNumEndpoints*/
  0x03,         /*bInterfaceClass: HID*/
  0x01,         /*bInterfaceSubClass : 1=BOOT, 0=no boot*/
  0x01,         /*modified: do not generate code, nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse*/
  0,            /*iInterface: Index of string descriptor*/
  /* HID Descriptor */
  0x09,                       /* bLength: HID Descriptor size */
  HID_DESCRIPTOR_TYPE, 			/* bDescriptorType: HID */
  0x11,                       /* bcdHID: HID Class Spec release number (1.11) */
  0x01,
  0x00,                       /* bCountryCode: Hardware target country (0 = not localized) */
  0x01,                       /* bNumDescriptors: Number of HID class descriptors to follow */
  0x22,                       /* bDescriptorType: Report descriptor type */
  HID_KEYBOARD_REPORT_DESC_SIZE,/* wItemLength: Total length of Report descriptor */
  0x00,

  /* Endpoint 1 IN Descriptor - HID Report (keep existing, HID currently uses this) */
  0x07,                       /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,     /* bDescriptorType: Endpoint */
  HID_EPIN_ADDR,                       /* bEndpointAddress: EP1 IN (0x81 = IN, endpoint 1) */
  0x03,                       /* bmAttributes: Interrupt */
  HID_EPIN_SIZE,             /* wMaxPacketSize: 8 bytes */
  0x00,
  HID_FS_BINTERVAL,                        /* bInterval: Polling Interval (1 ms = 0x01) */
};

#if defined ( __ICCARM__ ) /* IAR Compiler */
  #pragma data_alignment=4
#endif /* defined ( __ICCARM__ ) */

/** USB lang identifier descriptor. */
__ALIGN_BEGIN uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END =
{
  USB_LEN_LANGID_STR_DESC,
  USB_DESC_TYPE_STRING,
  LOBYTE(USBD_LANGID_STRING),
  HIBYTE(USBD_LANGID_STRING)
};

#if defined ( __ICCARM__ ) /* IAR Compiler */
  #pragma data_alignment=4
#endif /* defined ( __ICCARM__ ) */
/* Internal string descriptor. */
__ALIGN_BEGIN uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN uint8_t USBD_StringSerial[USB_SIZ_STRING_SERIAL] __ALIGN_END =
{
  USB_SIZ_STRING_SERIAL,
  USB_DESC_TYPE_STRING,
};

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_DESC_Private_Functions USBD_COMPOSITE_DESC_Private_Functions
  * @brief Private functions.
  * @{
  */

/**
  * @brief  Return the device descriptor
  * @param  speed : Current device speed
  * @param  length : Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t * USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = sizeof(USBD_Composite_FS_DeviceDesc);
  return USBD_Composite_FS_DeviceDesc;
}

/**
  * @brief  Return the LangID string descriptor
  * @param  speed : Current device speed
  * @param  length : Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t * USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = sizeof(USBD_LangIDDesc);
  return USBD_LangIDDesc;
}

/**
  * @brief  Return the product string descriptor
  * @param  speed : Current device speed
  * @param  length : Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t * USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  if(speed == 0)
  {
    USBD_GetString((uint8_t *)USBD_PRODUCT_STRING_FS, USBD_StrDesc, length);
  }
  else
  {
    USBD_GetString((uint8_t *)USBD_PRODUCT_STRING_FS, USBD_StrDesc, length);
  }
  return USBD_StrDesc;
}

/**
  * @brief  Return the manufacturer string descriptor
  * @param  speed : Current device speed
  * @param  length : Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t * USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
  return USBD_StrDesc;
}

/**
  * @brief  Return the serial number string descriptor
  * @param  speed : Current device speed
  * @param  length : Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t * USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = USB_SIZ_STRING_SERIAL;

  /* Update the serial number string descriptor with the data from the unique
   * ID */
  Get_SerialNum();
  /* USER CODE BEGIN USBD_FS_SerialStrDescriptor */

  /* USER CODE END USBD_FS_SerialStrDescriptor */
  return (uint8_t *) USBD_StringSerial;
}

/**
  * @brief  Return the configuration string descriptor
  * @param  speed : Current device speed
  * @param  length : Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t * USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  if(speed == USBD_SPEED_HIGH)
  {
    USBD_GetString((uint8_t *)USBD_CONFIGURATION_STRING_FS, USBD_StrDesc, length);
  }
  else
  {
    USBD_GetString((uint8_t *)USBD_CONFIGURATION_STRING_FS, USBD_StrDesc, length);
  }
  return USBD_StrDesc;
}

/**
  * @brief  Return the interface string descriptor
  * @param  speed : Current device speed
  * @param  length : Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t * USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  if(speed == 0)
  {
    uint8_t idx = hUsbDeviceFS.request.wIndex & 0xFF;
    if(idx == 0)
    {
      USBD_GetString((uint8_t *)USBD_INTERFACE_STRING_CDC_CONTROL_FS, USBD_StrDesc, length);
    }
    else if(idx == 1)
    {
      USBD_GetString((uint8_t *)USBD_INTERFACE_STRING_CDC_DATA_FS, USBD_StrDesc, length);
    }
    else
    {
      USBD_GetString((uint8_t *)USBD_INTERFACE_STRING_HID_FS, USBD_StrDesc, length);
    }
  }
  else
  {
    USBD_GetString((uint8_t *)USBD_INTERFACE_STRING_HID_FS, USBD_StrDesc, length);
  }
  return USBD_StrDesc;
}

/**
  * @brief  Create the serial number string descriptor
  * @param  None
  * @retval None
  */
static void Get_SerialNum(void)
{
  uint32_t deviceserial0 = *(uint32_t *)DEVICE_ID1;
  uint32_t deviceserial1 = *(uint32_t *)DEVICE_ID2;
  uint32_t deviceserial2 = *(uint32_t *)DEVICE_ID3;

  deviceserial0 += deviceserial2;

  if (deviceserial0 != 0)
  {
    IntTo_Unicode(deviceserial0, &USBD_StringSerial[2], 8);
    IntTo_Unicode(deviceserial1, &USBD_StringSerial[18], 4);
  }
}

/**
  * @brief  Convert Hex 32Bits value into char
  * @param  value: value to convert
  * @param  pbuf: pointer to the buffer
  * @param  len: buffer length
  * @retval None
  */
static void IntTo_Unicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
  uint8_t idx = 0u;

  for (idx = 0u; idx < len; idx++)
  {
    if (((value >> 28)) < 0xA)
    {
      pbuf[2u * idx] = (value >> 28) + '0';
    }
    else
    {
      pbuf[2u * idx] = (value >> 28) + 'A' - 10u;
    }

    value = value << 4u;

    pbuf[2u * idx + 1u] = 0u;
  }
}
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
