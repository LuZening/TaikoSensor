/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    : usbd_composite_wrapper.c
  * @brief   : USB Composite Device Wrapper Implementation (CDC + HID)
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
#include "usbd_composite_wrapper.h"
#include "usbd_drumcontroller_wrapper.h"
#include "usbd_cdc.h"
#include "usbd_hid.h"
#include "usbd_composite_desc.h"

#include "usb_cdc_wrapper.h"
#include "usbd_cdc_if.h"

/* USER CODE BEGIN INCLUDE */

/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
volatile uint8_t g_HID_report_modified = 0;
/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief Usb device library.
  * @{
  */

/** @addtogroup USBD_COMPOSITE_WRAPPER
  * @{
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Private_TypesDefinitions USBD_COMPOSITE_WRAPPER_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Private_Defines USBD_COMPOSITE_WRAPPER_Private_Defines
  * @brief Private defines.
  * @{
  */

/* USER CODE BEGIN PRIVATE_DEFINES */

/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Private_Macros USBD_COMPOSITE_WRAPPER_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Private_FunctionPrototypes USBD_COMPOSITE_WRAPPER_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

static uint8_t USBD_Composite_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_Composite_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_Composite_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_Composite_EP0_TxSent(USBD_HandleTypeDef *pdev);
static uint8_t USBD_Composite_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t USBD_Composite_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_Composite_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_Composite_SOF(USBD_HandleTypeDef *pdev);
static uint8_t *USBD_Composite_GetFSConfigDescriptor(uint16_t *length);
static uint8_t *USBD_Composite_GetHSConfigDescriptor(uint16_t *length);
static uint8_t *USBD_Composite_GetDeviceQualifierDescriptor(uint16_t *length);

#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
static uint8_t *USBD_Composite_GetUsrStrDescriptor(USBD_HandleTypeDef *pdev, uint8_t index, uint16_t *length);
#endif

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Private_Variables USBD_COMPOSITE_WRAPPER_Private_Variables
  * @brief Private variables.
  * @{
  */

/* Structure for composite class handling */
USBD_ClassTypeDef USBD_COMPOSITE =
{
  USBD_Composite_Init,
  USBD_Composite_DeInit,
  USBD_Composite_Setup,
  USBD_Composite_EP0_TxSent,
  USBD_Composite_EP0_RxReady,
  USBD_Composite_DataIn,
  USBD_Composite_DataOut,
  USBD_Composite_SOF,
  NULL,
  NULL,
  USBD_Composite_GetHSConfigDescriptor,
  USBD_Composite_GetFSConfigDescriptor,
  USBD_Composite_GetDeviceQualifierDescriptor,
#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
  USBD_Composite_GetUsrStrDescriptor
#endif
};

/* USER CODE BEGIN PRIVATE_VARIABLES */

/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Exported_Variables USBD_COMPOSITE_WRAPPER_Exported_Variables
  * @brief Public variables.
  * @{
  */

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_COMPOSITE_WRAPPER_Private_Functions USBD_COMPOSITE_WRAPPER_Private_Functions
  * @brief Private functions.
  * @{
  */

/**
  * @brief  USBD_Composite_Init
  *         Initialize the CDC and HID classes
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_Composite_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  uint8_t res = USBD_OK;
  USBD_Composite_HandleTypeDef *hcomp;

  /* Allocate memory for composite handle */
  hcomp = (USBD_Composite_HandleTypeDef *)USBD_malloc(sizeof(USBD_Composite_HandleTypeDef));
  if(hcomp == NULL)
  {
    pdev->pClassData = NULL;
    return USBD_FAIL;
  }
  /** TYPE 0: DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE**/
  if(g_USB_device_type == DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE)
  {
	  /* Initialize HID class */
	  hcomp->HID_ClassId = 0;
	  pdev->pClassData = (void *)&hcomp->hhid;
	  pdev->pHID = (void *)&hcomp->hhid;
	  pdev->pUserData = NULL;
	  res = USBD_HID.Init(pdev, cfgidx);

	  /* Initialize CDC class */
	  pdev->pClassData = (void *)&hcomp->hcdc;
	  pdev->pCDC = (void *)&hcomp->hcdc;
	  pdev->pUserData = &USBD_CDC_Interface_fops_FS;
	  hcomp->CDC_ClassId = 1;
	  res = USBD_CDC.Init(pdev, cfgidx);
	  if(res != USBD_OK)
	  {
		return res;
	  }
  }
  /* Restore composite handle */
  pdev->pClassData = (void *)hcomp;

  return res;
}

/**
  * @brief  USBD_Composite_DeInit
  *         DeInitialize the CDC and HID classes
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_Composite_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  uint8_t res = USBD_OK;
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassData;

  if(hcomp != NULL)
  {
	  /** TYPE 0: DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE**/
	  if(g_USB_device_type == DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE)
	  {
		/* DeInitialize CDC */
		pdev->pClassData = (void *)&hcomp->hcdc;
		USBD_CDC.DeInit(pdev, cfgidx);

		/* DeInitialize HID */
		pdev->pClassData = (void *)&hcomp->hhid;
		USBD_HID.DeInit(pdev, cfgidx);

		/* Free composite handle */
		USBD_free((void *)hcomp);
		pdev->pClassData = NULL;
	  }

  }

  return res;
}

/**
  * @brief  USBD_Composite_Setup
  *         Handle the setup requests for CDC and HID classes
  * @param  pdev: device instance
  * @param  req: USB request
  * @retval status
  */
static uint8_t USBD_Composite_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassData;
  uint8_t interface = LOBYTE(req->wIndex);
  uint8_t result = USBD_OK;

  /* Determine which class to route to based on interface number */
  if(interface == COMPOSITE_CDC_CONTROL_INTERFACE || interface == COMPOSITE_CDC_DATA_INTERFACE)
  {
    /* Route to CDC */
    pdev->pClassData = (void *)&hcomp->hcdc;
    pdev->pUserData = &USBD_CDC_Interface_fops_FS;
    result = USBD_CDC.Setup(pdev, req);
  }
  else if(interface == COMPOSITE_HID_INTERFACE)
  {
    /* Route to HID */
	  if(g_USB_device_type == DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE)
	  {
		pdev->pClassData = (void *)&hcomp->hhid;
		result = USBD_HID.Setup(pdev, req);
	  }
  }
  else
  {
    USBD_CtlError(pdev, req);
    result = USBD_FAIL;
  }

  /* Restore composite handle */
  pdev->pClassData = (void *)hcomp;

  return result;
}

/**
  * @brief  USBD_Composite_EP0_TxSent
  *         Handle EP0 TxSent event
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t USBD_Composite_EP0_TxSent(USBD_HandleTypeDef *pdev)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassData;

  /* Both CDC and HID may need EP0 TxSent */
  /* For safety, call both (they check their own states) */
  if(g_USB_device_type == DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE)
  {
	  pdev->pClassData = (void *)&hcomp->hcdc;
	  if(USBD_CDC.EP0_TxSent)
		  USBD_CDC.EP0_TxSent(pdev);
	  pdev->pClassData = (void *)&hcomp->hhid;
	  if(USBD_HID.EP0_TxSent)
		  USBD_HID.EP0_TxSent(pdev);
  }


  pdev->pClassData = (void *)hcomp;

  return USBD_OK;
}

/**
  * @brief  USBD_Composite_EP0_RxReady
  *         Handle EP0 RxReady event
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t USBD_Composite_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassData;

  if(g_USB_device_type == DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE)
  {
	  /* Route to CDC for line coding requests */
	  pdev->pClassData = (void *)&hcomp->hcdc;
	  pdev->pUserData = &USBD_CDC_Interface_fops_FS;
	  USBD_CDC.EP0_RxReady(pdev);

	  pdev->pClassData = (void *)&hcomp->hhid;
	  pdev->pUserData = NULL;
	  if(USBD_HID.EP0_RxReady)
		  USBD_CDC.EP0_RxReady(pdev);
  }


  pdev->pClassData = (void *)hcomp;

  return USBD_OK;
}

/**
  * @brief  USBD_Composite_DataIn
  *         Handle data IN stage for CDC and HID
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t USBD_Composite_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassData;
  uint8_t result = USBD_OK;

  /* Route based on endpoint number */
  if((epnum == (CDC_IN_EP & 0x7F)) || (epnum == (CDC_CMD_EP & 0x7F)))
  {
    /* CDC endpoints */
    pdev->pClassData = (void *)&hcomp->hcdc;
    result = USBD_CDC.DataIn(pdev, epnum);
  }
  else if(epnum == (HID_IN_EP & 0x7F))
  {
    if(g_USB_device_type == DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE)
    {
		/* HID endpoint */
		pdev->pClassData = (void *)&hcomp->hhid;
		result = USBD_HID.DataIn(pdev, epnum);
    }

  }

  pdev->pClassData = (void *)hcomp;

  return result;
}

/**
  * @brief  USBD_Composite_DataOut
  *         Handle data OUT stage for CDC
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t USBD_Composite_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassData;
  uint8_t result = USBD_OK;

  /* Route based on endpoint number */
  if(epnum == (CDC_OUT_EP & 0x7F))
  {
    /* CDC OUT endpoint */
    pdev->pClassData = (void *)&hcomp->hcdc;
    result = USBD_CDC.DataOut(pdev, epnum);
  }
  else if(epnum == (HID_IN_EP & 0x7F))
  {
    if(g_USB_device_type == DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE)
    {
		/* HID OUT endpoint (if used) */
		pdev->pClassData = (void *)&hcomp->hhid;
		if(USBD_HID.DataOut)
			result = USBD_HID.DataOut(pdev, epnum);
    }

  }

  pdev->pClassData = (void *)hcomp;

  return result;
}

/**
  * @brief  USBD_Composite_SOF
  *         Handle SOF event
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t USBD_Composite_SOF(USBD_HandleTypeDef *pdev)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassData;
  uint8_t result = USBD_OK;

  if(g_USB_device_type == DEVICE_TYPE_KEYBOARD_CDC_COMPOSITE)
  {
	  /* Call HID SOF (CDC doesn't use SOF) */
	  pdev->pClassData = (void *)&hcomp->hhid;
	  if(USBD_HID.SOF)
		  result = USBD_HID.SOF(pdev);
  }


  pdev->pClassData = (void *)hcomp;

  return result;
}

/**
  * @brief  USBD_Composite_GetFSConfigDescriptor
  *         Return configuration descriptor
  * @param  length: pointer to data length variable
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_Composite_GetFSConfigDescriptor(uint16_t *length)
{
  *length = USB_COMPOSITE_CONFIG_DESC_SIZ;
  return USBD_Composite_FS_CfgDesc;
}

/**
  * @brief  USBD_Composite_GetHSConfigDescriptor
  *         Return configuration descriptor
  * @param  length: pointer to data length variable
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_Composite_GetHSConfigDescriptor(uint16_t *length)
{
	  *length = USB_COMPOSITE_CONFIG_DESC_SIZ;
	  return USBD_Composite_FS_CfgDesc;
}

/**
  * @brief  USBD_Composite_GetDeviceQualifierDescriptor
  *         Return Device Qualifier descriptor
  * @param  length: pointer to data length variable
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_Composite_GetDeviceQualifierDescriptor(uint16_t *length)
{
  *length = USB_LEN_DEV_QUALIFIER_DESC;
  /* Return NULL as device qualifier is optional for composite device */
  return NULL;
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
