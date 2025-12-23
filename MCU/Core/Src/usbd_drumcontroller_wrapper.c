
/* Includes ------------------------------------------------------------------*/
#include "usbd_drumcontroller_wrapper.h"
#include "usbd_ctlreq.h"
#include "usbd_def.h"  // For USBD_LANGID_STRING and other USB definitions

#include "usbd_composite_desc.h"
#include "usbd_composite_wrapper.h"
#include "usbd_hid.h"



/** @defgroup USBD_COMPOSITE_DESC_Private_FunctionPrototypes USBD_COMPOSITE_DESC_Private_FunctionPrototypes
  * @brief Private functions declaration for FS.
  * @{
  */



/*------------- Specific descriptors for DrumController mode (compatible with NS and PC, No CDC)----------------------------------------------------*/
__ALIGN_BEGIN uint8_t USBD_DrumController_FS_DeviceDesc[USB_LEN_COMPOSITE_DEV_DESC] __ALIGN_END =
{
  0x12,                       /*bLength */
  USB_DESC_TYPE_DEVICE,       /*bDescriptorType*/
  0x00,                       /*bcdUSB */
  0x02,
  0xEF,                       /*bDeviceClass (0xEF = Miscellaneous Device Class)*/
  0x02,                       /*bDeviceSubClass (2 = Common Class)*/
  0x01,                       /*bDeviceProtocol (1 = Interface Association Descriptor)*/
  USB_MAX_EP0_SIZE,           /*bMaxPacketSize*/
  LOBYTE(USBD_VID_DRUMCONTROLLER),           /*idVendor*/
  HIBYTE(USBD_VID_DRUMCONTROLLER),           /*idVendor*/
  LOBYTE(USBD_PID_DRUMCONTROLLER),        /*idProduct*/
  HIBYTE(USBD_PID_DRUMCONTROLLER),        /*idProduct*/
  0x00,                       /*bcdDevice rel. 2.00*/
  0x02,
  USBD_IDX_MFC_STR,           /*Index of manufacturer string*/
  USBD_IDX_PRODUCT_STR,       /*Index of product string*/
  USBD_IDX_SERIAL_STR,        /*Index of serial number string*/
  USBD_MAX_NUM_CONFIGURATION  /*bNumConfigurations*/
};

/** USB lang identifier descriptor. */
static __ALIGN_BEGIN uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END =
{
  USB_LEN_LANGID_STR_DESC,
  USB_DESC_TYPE_STRING,
  LOBYTE(USBD_LANGID_STRING_DRUMCONTROLLER),
  HIBYTE(USBD_LANGID_STRING_DRUMCONTROLLER)
};

/* Internal string descriptor. */
static __ALIGN_BEGIN uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

static __ALIGN_BEGIN uint8_t USBD_StringSerial[USB_SIZ_STRING_SERIAL] __ALIGN_END =
{
  USB_SIZ_STRING_SERIAL,
  USB_DESC_TYPE_STRING,
};

static uint8_t * USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	  UNUSED(speed);
	  *length = sizeof(USBD_DrumController_FS_DeviceDesc);
	  return USBD_DrumController_FS_DeviceDesc;
}

static uint8_t * USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	  UNUSED(speed);
	  *length = sizeof(USBD_LangIDDesc);
	  return USBD_LangIDDesc;
}

static uint8_t * USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	  if(speed == 0)
	  {
	    USBD_GetString((uint8_t *)USBD_DRUMCONTROLLER_PRODUCT_STRING_FS, USBD_StrDesc, length);
	  }
	  else
	  {
	    USBD_GetString((uint8_t *)USBD_DRUMCONTROLLER_PRODUCT_STRING_FS, USBD_StrDesc, length);
	  }
	  return USBD_StrDesc;
}

static uint8_t * USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	  UNUSED(speed);
	  USBD_GetString((uint8_t *)USBD_DRUMCONTROLLER_MANUFACTURER_STRING, USBD_StrDesc, length);
	  return USBD_StrDesc;
}

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


static uint8_t * USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
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

static uint8_t * USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	  if(speed == USBD_SPEED_HIGH)
	  {
	    USBD_GetString((uint8_t *)USBD_DRUMCONTROLLER_CONFIGURATION_STRING_FS, USBD_StrDesc, length);
	  }
	  else
	  {
	    USBD_GetString((uint8_t *)USBD_DRUMCONTROLLER_CONFIGURATION_STRING_FS, USBD_StrDesc, length);
	  }
	  return USBD_StrDesc;
}

static uint8_t * USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    // For drum controller mode, we only have one interface (index 0)
    USBD_GetString((uint8_t *)USBD_DRUMCONTROLLER_INTERFACE_STRING_HID_FS, USBD_StrDesc, length);
    return USBD_StrDesc;
}


/** @defgroup USBD_COMPOSITE_DESC_Private_Variables USBD_COMPOSITE_DESC_Private_Variables
  * @brief Private variables.
  * @{
  */

USBD_DescriptorsTypeDef FS_DRUMCONTROLLER_Desc =
{
  USBD_FS_DeviceDescriptor
, USBD_FS_LangIDStrDescriptor
, USBD_FS_ManufacturerStrDescriptor
, USBD_FS_ProductStrDescriptor
, USBD_FS_SerialStrDescriptor
, USBD_FS_ConfigStrDescriptor
, USBD_FS_InterfaceStrDescriptor
};

static uint8_t  USBD_DrumController_Init(USBD_HandleTypeDef *pdev,
                              uint8_t cfgidx);

static uint8_t  USBD_DrumController_DeInit(USBD_HandleTypeDef *pdev,
                                uint8_t cfgidx);

static uint8_t  USBD_DrumController_Setup(USBD_HandleTypeDef *pdev,
                               USBD_SetupReqTypedef *req);

static uint8_t  *USBD_DrumController_GetFSCfgDesc(uint16_t *length);

static uint8_t  *USBD_DrumController_GetHSCfgDesc(uint16_t *length);

static uint8_t  *USBD_DrumController_GetOtherSpeedCfgDesc(uint16_t *length);

static uint8_t  *USBD_DrumController_GetDeviceQualifierDesc(uint16_t *length);

static uint8_t  USBD_DrumController_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_DrumController_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
/**
  * @}
  */

/** @defgroup USBD_HID_Private_Variables
  * @{
  */

USBD_ClassTypeDef  USBD_DRUMCONTROLLER =
{
  USBD_DrumController_Init,
  USBD_DrumController_DeInit,
  USBD_DrumController_Setup,
  NULL, /*EP0_TxSent*/
  NULL, /*EP0_RxReady*/
  USBD_DrumController_DataIn, /*DataIn*/
  USBD_DrumController_DataOut, /*DataOut*/
  NULL, /*SOF */
  NULL,
  NULL,
  USBD_DrumController_GetHSCfgDesc,
  USBD_DrumController_GetFSCfgDesc,
  USBD_DrumController_GetOtherSpeedCfgDesc,
  USBD_DrumController_GetDeviceQualifierDesc,
};



__ALIGN_BEGIN uint8_t USBD_DrumController_FS_CfgDesc[USB_DRUMCONTROLLER_CONFIG_DESC_SIZ] __ALIGN_END =
{
  /* Configuration Descriptor */
  0x09,                       /* bLength: Configuration Descriptor size */
  USB_DESC_TYPE_CONFIGURATION, /* bDescriptorType: Configuration */
  USB_DRUMCONTROLLER_CONFIG_DESC_SIZ, /* wTotalLength: Total size of data */
  0x00,
  0x01,                       /* bNumInterfaces: 1 interface1 HID (1 EP) */
  0x01,                       /* bConfigurationValue: Configuration value */
  0x00,                       /* iConfiguration: Index of string descriptor */
  0xC0,                       /* bmAttributes: self powered */
  0x32,                       /* MaxPower 100 mA */

  /*--------------------------- INTERFACE 0 for HID ---------------------------------------------*/
  /* HID Interface Descriptor */
  0x09,         /*bLength: Interface Descriptor size*/
  USB_DESC_TYPE_INTERFACE,/*bDescriptorType: Interface descriptor type*/
  COMPOSITE_HID_INTERFACE,         /*bInterfaceNumber: Number of Interface*/
  0x00,         /*bAlternateSetting: Alternate setting*/
  0x02,         /*bNumEndpoints*/
  0x03,         /*bInterfaceClass: HID*/
  0x00,         /*bInterfaceSubClass : 1=BOOT, 0=no boot*/
  0x00,         /*modified: do not generate code, nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse*/
  0,            /*iInterface: Index of string descriptor*/

  /* HID Descriptor */
  0x09,                       /* bLength: HID Descriptor size */
  HID_DESCRIPTOR_TYPE, 			/* bDescriptorType: HID */
  0x11,                       /* bcdHID: HID Class Spec release number (1.11) */
  0x01,
  0x00,                       /* bCountryCode: Hardware target country (0 = not localized) */
  0x01,                       /* bNumDescriptors: Number of HID class descriptors to follow */
  0x22,                       /* bDescriptorType: Report descriptor type */
  HID_DRUMCONTROLLER_REPORT_DESC_SIZE,/* wItemLength: Total length of Report descriptor */
  0x00,

  /* Endpoint 1 IN Descriptor - HID Report (keep existing, HID currently uses this) */
  0x07,                       /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,     /* bDescriptorType: Endpoint */
  HID_DRUMCONTROLLER_EPIN_ADDR,                       /* bEndpointAddress: EP1 IN (0x81 = IN, endpoint 1) */
  0x03,                       /* bmAttributes: Interrupt */
  HID_DRUMCONTROLLER_EPIN_SIZE,             /* wMaxPacketSize: 8 bytes */
  0x00,
  0x05,                        /* bInterval: Polling Interval (8 ms = 0x08) */

  /* Endpoint 2 OUT Descriptor - HID Report (keep existing, HID currently uses this) */
  0x07,                       /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,     /* bDescriptorType: Endpoint */
  HID_DRUMCONTROLLER_EPOUT_ADDR,                       /* bEndpointAddress: EP2 OUT (0x02 = OUT, endpoint 2) */
  0x03,                       /* bmAttributes: Interrupt */
  HID_DRUMCONTROLLER_EPOUT_SIZE,             /* wMaxPacketSize: 64 bytes */
  0x00,
  0x05,                        /* bInterval: Polling Interval */

};
/*-----------------------------------------------------------------------------*/

__ALIGN_BEGIN static uint8_t HID_DRUMCONTROLLER_ReportDesc[HID_DRUMCONTROLLER_REPORT_DESC_SIZE]  __ALIGN_END =
{
	    0x05, 0x01,        //   USAGE_PAGE (Generic Desktop)
	    0x09, 0x05,        //   USAGE (Game Pad)
	    0xa1, 0x01,        //   COLLECTION (Application)
	    0x15, 0x00,        //   LOGICAL_MINIMUM (0)
	    0x25, 0x01,        //   LOGICAL_MAXIMUM (1)
	    0x35, 0x00,        //   PHYSICAL_MINIMUM (0)
	    0x45, 0x01,        //   PHYSICAL_MAXIMUM (1)
	    0x75, 0x01,        //   REPORT_SIZE (1)
	    0x95, 0x10,        //   REPORT_COUNT (16)
	    0x05, 0x09,        //   USAGE_PAGE (Button)
	    0x19, 0x01,        //   USAGE_MINIMUM (1)
	    0x29, 0x10,        //   USAGE_MAXIMUM (16)
	    0x81, 0x02,        //   INPUT (Data,Var,Abs)
	    0x05, 0x01,        //   USAGE_PAGE (Generic Desktop)
	    0x25, 0x07,        //   LOGICAL_MAXIMUM (7)
	    0x46, 0x3b, 0x01,  //   PHYSICAL_MAXIMUM (315)
	    0x75, 0x04,        //   REPORT_SIZE (4)
	    0x95, 0x01,        //   REPORT_COUNT (1)
	    0x65, 0x14,        //   UNIT (20)
	    0x09, 0x39,        //   USAGE (Hat Switch)
	    0x81, 0x42,        //   INPUT (Data,Var,Abs)
	    0x65, 0x00,        //   UNIT (0)
	    0x95, 0x01,        //   REPORT_COUNT (1)
	    0x81, 0x01,        //   INPUT (Cnst,Arr,Abs)
	    0x26, 0xff, 0x00,  //   LOGICAL_MAXIMUM (255)
	    0x46, 0xff, 0x00,  //   PHYSICAL_MAXIMUM (255)
	    0x09, 0x30,        //   USAGE (X)
	    0x09, 0x31,        //   USAGE (Y)
	    0x09, 0x32,        //   USAGE (Z)
	    0x09, 0x35,        //   USAGE (Rz)
	    0x75, 0x08,        //   REPORT_SIZE (8)
	    0x95, 0x04,        //   REPORT_COUNT (4)
	    0x81, 0x02,        //   INPUT (Data,Var,Abs)
	    0x06, 0x00, 0xff,  //   USAGE_PAGE (Vendor Defined 65280)
	    0x09, 0x20,        //   USAGE (32)
	    0x95, 0x01,        //   REPORT_COUNT (1)
	    0x81, 0x02,        //   INPUT (Data,Var,Abs)
	    0x0a, 0x21, 0x26,  //   USAGE (9761)
	    0x95, 0x08,        //   REPORT_COUNT (8)
	    0x91, 0x02,        //   OUTPUT (Data,Var,Abs)
	    0xc0               // END_COLLECTION
		// 86 bytes
};

/* USB HID device Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_DRUMCONTROLLER_HID_Desc[USB_HID_DESC_SIZ]  __ALIGN_END  =
{
  /* 18 */
  0x09,         /*bLength: HID Descriptor size*/
  HID_DESCRIPTOR_TYPE, /*bDescriptorType: HID*/
  0x11,         /*bcdHID: HID Class Spec release number*/
  0x01,
  0x00,         /*bCountryCode: Hardware target country*/
  0x01,         /*bNumDescriptors: Number of HID class descriptors to follow*/
  0x22,         /*bDescriptorType*/
  HID_DRUMCONTROLLER_REPORT_DESC_SIZE,/*wItemLength: Total length of Report descriptor*/
  0x00,
};
/**
  * @}
  */

/** @defgroup USBD_HID_Private_Functions
  * @{
  */

/**
  * @brief  USBD_HID_Init
  *         Initialize the HID interface
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t  USBD_DrumController_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  USBD_Composite_HandleTypeDef *hcomp;
  /* Allocate memory for composite handle */
  hcomp = (USBD_Composite_HandleTypeDef *)USBD_malloc(sizeof(USBD_Composite_HandleTypeDef));
  if(hcomp == NULL)
  {
    pdev->pClassData = NULL;
    return USBD_FAIL;
  }
  pdev->pClassData = (void *)&hcomp->hhid;
  pdev->pHID = (void *)&hcomp->hhid;
  pdev->pUserData = NULL;

  /* Open EP IN */
  USBD_LL_OpenEP(pdev, HID_DRUMCONTROLLER_EPIN_ADDR, USBD_EP_TYPE_INTR, HID_DRUMCONTROLLER_EPIN_SIZE);
  pdev->ep_in[HID_DRUMCONTROLLER_EPIN_SIZE & 0xFU].is_used = 1U;
  ((USBD_HID_HandleTypeDef *)(pdev->pHID))->state = HID_IDLE;

  return USBD_OK;
}

/**
  * @brief  USBD_HID_Init
  *         DeInitialize the HID layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t  USBD_DrumController_DeInit(USBD_HandleTypeDef *pdev,
                                uint8_t cfgidx)
{
  /* Close HID EPs */
  USBD_LL_CloseEP(pdev, HID_DRUMCONTROLLER_EPIN_ADDR);
  pdev->ep_in[HID_DRUMCONTROLLER_EPIN_ADDR & 0xFU].is_used = 0U;

	/* DeInitialize HID */
	USBD_DRUMCONTROLLER.DeInit(pdev, cfgidx);

	pdev->pClassData = NULL;

  return USBD_OK;
}

/**
  * @brief  USBD_HID_Setup
  *         Handle the HID specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t  USBD_DrumController_Setup(USBD_HandleTypeDef *pdev,
                               USBD_SetupReqTypedef *req)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)(pdev->pHID);
  uint16_t len = 0U;
  uint8_t *pbuf = NULL;
  uint16_t status_info = 0U;
  USBD_StatusTypeDef ret = USBD_OK;

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    case USB_REQ_TYPE_CLASS :
      switch (req->bRequest)
      {
        case HID_REQ_SET_PROTOCOL:
          hhid->Protocol = (uint8_t)(req->wValue);
          break;

        case HID_REQ_GET_PROTOCOL:
          USBD_CtlSendData(pdev, (uint8_t *)(void *)&hhid->Protocol, 1U);
          break;

        case HID_REQ_SET_IDLE:
          hhid->IdleState = (uint8_t)(req->wValue >> 8);
          break;

        case HID_REQ_GET_IDLE:
          USBD_CtlSendData(pdev, (uint8_t *)(void *)&hhid->IdleState, 1U);
          break;

        default:
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;
    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
        case USB_REQ_GET_STATUS:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            USBD_CtlSendData(pdev, (uint8_t *)(void *)&status_info, 2U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_GET_DESCRIPTOR:
        	/* HOST request for HID report desc */
          if (req->wValue >> 8 == HID_REPORT_DESC)
          {
            len = MIN(HID_DRUMCONTROLLER_REPORT_DESC_SIZE, req->wLength);
            pbuf = HID_DRUMCONTROLLER_ReportDesc;
          }
          /* HOST request for HID desc (the 9 bytes thingy) */
          else if (req->wValue >> 8 == HID_DESCRIPTOR_TYPE)
          {
            pbuf = USBD_DRUMCONTROLLER_HID_Desc;
            len = MIN(USB_HID_DESC_SIZ, req->wLength);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
          }
          USBD_CtlSendData(pdev, pbuf, len);
          break;

        case USB_REQ_GET_INTERFACE :
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            USBD_CtlSendData(pdev, (uint8_t *)(void *)&hhid->AltSetting, 1U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_SET_INTERFACE :
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            hhid->AltSetting = (uint8_t)(req->wValue);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        default:
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;

    default:
      USBD_CtlError(pdev, req);
      ret = USBD_FAIL;
      break;
  }

  return ret;
}

/**
  * @brief  USBD_HID_SendReport
  *         Send HID Report
  * @param  pdev: device instance
  * @param  buff: pointer to report
  * @retval status
  */
uint8_t USBD_DrumController_SendReport(USBD_HandleTypeDef  *pdev,
                            uint8_t *report,
                            uint16_t len)
{
  USBD_HID_HandleTypeDef     *hhid = (USBD_HID_HandleTypeDef *)pdev->pHID;

  if (pdev->dev_state == USBD_STATE_CONFIGURED)
  {
    if (hhid->state == HID_IDLE)
    {
      hhid->state = HID_BUSY;
      USBD_LL_Transmit(pdev,
    		  HID_DRUMCONTROLLER_EPIN_ADDR,
                       report,
                       len);
    }
  }
  return USBD_OK;
}

/**
  * @brief  USBD_HID_GetPollingInterval
  *         return polling interval from endpoint descriptor
  * @param  pdev: device instance
  * @retval polling interval
  */
uint32_t USBD_DrumController_GetPollingInterval(USBD_HandleTypeDef *pdev)
{
  uint32_t polling_interval = 0U;

  /* HIGH-speed endpoints */
  if (pdev->dev_speed == USBD_SPEED_HIGH)
  {
    /* Sets the data transfer polling interval for high speed transfers.
     Values between 1..16 are allowed. Values correspond to interval
     of 2 ^ (bInterval-1). This option (8 ms, corresponds to HID_HS_BINTERVAL */
    polling_interval = (((1U << (HID_HS_BINTERVAL - 1U))) / 8U);
  }
  else   /* LOW and FULL-speed endpoints */
  {
    /* Sets the data transfer polling interval for low and full
    speed transfers */
    polling_interval =  HID_FS_BINTERVAL;
  }

  return ((uint32_t)(polling_interval));
}

/**
  * @brief  USBD_HID_GetCfgFSDesc
  *         return FS configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t  *USBD_DrumController_GetFSCfgDesc(uint16_t *length)
{
  *length = sizeof(USBD_DrumController_FS_CfgDesc);
  return USBD_DrumController_FS_CfgDesc;
}

/**
  * @brief  USBD_HID_GetCfgHSDesc
  *         return HS configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t  *USBD_DrumController_GetHSCfgDesc(uint16_t *length)
{
  *length = sizeof(USBD_DrumController_FS_CfgDesc);
  return USBD_DrumController_FS_CfgDesc;
}

/**
  * @brief  USBD_HID_GetOtherSpeedCfgDesc
  *         return other speed configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t  *USBD_DrumController_GetOtherSpeedCfgDesc(uint16_t *length)
{
  *length = sizeof(USBD_DrumController_FS_CfgDesc);
  return USBD_DrumController_FS_CfgDesc;
}

/**
  * @brief  USBD_HID_DataIn
  *         handle data IN Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t  USBD_DrumController_DataIn(USBD_HandleTypeDef *pdev,
                                uint8_t epnum)
{

  /* Ensure that the FIFO is empty before a new transfer, this condition could
  be caused by  a new transfer before the end of the previous transfer */
  ((USBD_HID_HandleTypeDef *)pdev->pHID)->state = HID_IDLE;
  return USBD_OK;
}

/**
  * @brief  USBD_DrumController_DataOut
  *         Handle data OUT Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t  USBD_DrumController_DataOut(USBD_HandleTypeDef *pdev,
                                 uint8_t epnum)
{
  /* This is a placeholder to match endpoint descriptor.
   * The drum controller doesn't typically process OUT data,
   * so we just return OK to acknowledge the transaction. */
  return USBD_OK;
}


/**
* @brief  DeviceQualifierDescriptor
*         return Device Qualifier descriptor
* @param  length : pointer data length
* @retval pointer to descriptor buffer
*/
static uint8_t  *USBD_DrumController_GetDeviceQualifierDesc(uint16_t *length)
{
  static __ALIGN_BEGIN uint8_t USBD_DRUMCONTROLLER_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
  {
    USB_LEN_DEV_QUALIFIER_DESC,                 /* bLength */
    USB_DESC_TYPE_DEVICE_QUALIFIER,             /* bDescriptorType */
    0x00,                                       /* bcdUSB */
    0x02,
    0x00,                                       /* bDeviceClass */
    0x00,                                       /* bDeviceSubClass */
    0x00,                                       /* bDeviceProtocol */
    0x40,                                       /* bMaxPacketSize */
    0x01,                                       /* bNumConfigurations */
    0x00                                        /* bReserved */
  };

  *length = sizeof(USBD_DRUMCONTROLLER_DeviceQualifierDesc);
  return USBD_DRUMCONTROLLER_DeviceQualifierDesc;
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
