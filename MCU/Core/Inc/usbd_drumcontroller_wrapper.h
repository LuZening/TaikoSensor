/*
 * usbd_drumcontroller_wrapper.h
 *
 *  Created on: 2025年12月14日
 *      Author: cpholzn
 */

#ifndef INC_USBD_DRUMCONTROLLER_WRAPPER_H_
#define INC_USBD_DRUMCONTROLLER_WRAPPER_H_

/* Includes ------------------------------------------------------------------*/
#include  "usbd_ioreq.h"


// TODO: do not forget to fill these values
#define USBD_VID_DRUMCONTROLLER     0x0F0DU
#define USBD_PID_DRUMCONTROLLER     0x0092U
#define USBD_DRUMCONTROLLER_MANUFACTURER_STRING "HORI"
#define USBD_DRUMCONTROLLER_PRODUCT_STRING_FS "Taiko Controller"
#define USBD_DRUMCONTROLLER_CONFIGURATION_STRING_FS "Gamepad Config"
#define USBD_DRUMCONTROLLER_INTERFACE_STRING_HID_FS "Gamepad"
#define USB_DRUMCONTROLLER_CONFIG_DESC_SIZ    41U
#define HID_DRUMCONTROLLER_REPORT_DESC_SIZE 	86U
#define HID_DRUMCONTROLLER_EPIN_ADDR                 0x81U
#define HID_DRUMCONTROLLER_EPIN_SIZE                 0x40U // 64Bytes
#define HID_DRUMCONTROLLER_EPOUT_ADDR                0x02U
#define HID_DRUMCONTROLLER_EPOUT_SIZE                0x40U // 64Bytes


extern USBD_ClassTypeDef  USBD_DRUMCONTROLLER;
extern USBD_DescriptorsTypeDef FS_DRUMCONTROLLER_Desc;
#define USBD_DRUMCONTROLLER_CLASS    &USBD_DRUMCONTROLLER

/* Function prototypes */
uint8_t USBD_DrumController_SendReport(USBD_HandleTypeDef *pdev,
                            uint8_t *report,
                            uint16_t len);

#endif /* INC_USBD_DRUMCONTROLLER_WRAPPER_H_ */
