/**
  ******************************************************************************
  * @file    : usb_cdc_wrapper.c
  * @brief   : This file implements the USB CDC Wrapper with endpoint remapping.
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

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc.h"
#include "usb_cdc_wrapper.h"
#include "usbd_cdc_if.h"
#include "uart_terminal.h"

/* USER CODE BEGIN INCLUDE */

/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
extern USBD_HandleTypeDef hUsbDeviceFS;

/* Line coding settings - required for CDC_GET_LINE_CODING */
static USBD_CDC_LineCodingTypeDef LineCoding =
{
  115200, /* baud rate */
  0x00,   /* 1 stop bit */
  0x00,   /* no parity */
  0x08    /* 8 data bits */
};

/* Ring buffer structures for USB CDC communication */

/* TX Ring Buffer (multi-writer safe) */
#define TX_RING_BUFFER_SIZE  2048U  /* 2KB for responses (help text) */
#define TX_RING_BUFFER_MASK (TX_RING_BUFFER_SIZE - 1U)
typedef struct {
    uint8_t buffer[TX_RING_BUFFER_SIZE];
    volatile uint16_t head;  /* Write index */
    volatile uint16_t tail;  /* Read index */
} tx_ring_buffer_t;

static tx_ring_buffer_t g_tx_ring_buffer = {0};

/* RX Ring Buffer (USB ISR -> main loop) */
#define RX_RING_BUFFER_SIZE  256U   /* 256 bytes for incoming commands */
#define RX_RING_BUFFER_MASK (RX_RING_BUFFER_SIZE - 1U)
typedef struct {
    uint8_t buffer[RX_RING_BUFFER_SIZE];
    volatile uint16_t head;  /* Write index */
    volatile uint16_t tail;  /* Read index */
} rx_ring_buffer_t;

static rx_ring_buffer_t g_rx_ring_buffer = {0};

/* Flag to track if USB TX is in progress */
static volatile bool g_usb_tx_busy = false;

/* USER CODE END PV */

/* USER CODE BEGIN 0 */
/** Received data over USB are stored in this buffer      */
uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];

/** Data to send over USB CDC are stored in this buffer   */
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];
/* USER CODE END 0 */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Ring Buffer Helper Functions */

/**
 * @brief Initialize both TX and RX ring buffers
 * @retval None
 */
static void ring_buffer_init(void)
{
  g_tx_ring_buffer.head = 0;
  g_tx_ring_buffer.tail = 0;
  g_rx_ring_buffer.head = 0;
  g_rx_ring_buffer.tail = 0;
  g_usb_tx_busy = false;
}

/**
 * @brief Check if ring buffer is empty
 * @param head Head index (write)
 * @param tail Tail index (read)
 * @retval true if empty, false otherwise
 */
static inline bool ring_buffer_is_empty(volatile uint16_t head, volatile uint16_t tail)
{
  return (head == tail);
}

/**
 * @brief Check if ring buffer is full
 * @param head Head index (write)
 * @param tail Tail index (read)
 * @param size Buffer size
 * @retval true if full, false otherwise
 */
static inline bool ring_buffer_is_full(volatile uint16_t head, volatile uint16_t tail, uint16_t size)
{
  return ((head + 1) % size) == tail;
}

/**
 * @brief Get available space in ring buffer
 * @param head Head index (write)
 * @param tail Tail index (read)
 * @param size Buffer size
 * @retval Number of bytes available to write
 */
static inline uint16_t ring_buffer_available(volatile uint16_t head, volatile uint16_t tail, uint16_t size)
{
  if (head >= tail) {
    return size - (head - tail) - 1;
  } else {
    return tail - head - 1;
  }
}

/**
 * @brief Get used space in ring buffer
 * @param head Head index (write)
 * @param tail Tail index (read)
 * @param size Buffer size
 * @retval Number of bytes ready to read
 */
static inline uint16_t ring_buffer_used(volatile uint16_t head, volatile uint16_t tail, uint16_t size)
{
  if (head >= tail) {
    return head - tail;
  } else {
    return size - (tail - head);
  }
}

/* TX Ring Buffer Functions (Multi-writer safe) */

/**
 * @brief Enqueue data to TX ring buffer (thread-safe for multi-writer)
 * @param data Data to enqueue
 * @param len Length of data
 * @retval true if enqueued successfully, false if buffer full
 */
static bool tx_ring_buffer_enqueue(const uint8_t* data, uint16_t len)
{
  uint16_t space_available;
  uint16_t i;

  /* Enter critical section (disable interrupts) */
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  /* Check if there's enough space */
  space_available = ring_buffer_available(g_tx_ring_buffer.head, g_tx_ring_buffer.tail, TX_RING_BUFFER_SIZE);

  if (space_available < len) {
    /* Not enough space, exit critical section and return error */
    __set_PRIMASK(primask);
    return false;
  }

  /* Copy data to buffer */
  for (i = 0; i < len; i++) {
    g_tx_ring_buffer.buffer[g_tx_ring_buffer.head] = data[i];
    g_tx_ring_buffer.head = (g_tx_ring_buffer.head + 1) & TX_RING_BUFFER_MASK;
  }

  /* Exit critical section (restore interrupt state) */
  __set_PRIMASK(primask);

  return true;
}

/**
 * @brief Dequeue data from TX ring buffer (single reader - main loop only)
 * @param buf Buffer to store dequeued data
 * @param max_len Maximum bytes to dequeue
 * @retval Number of bytes actually dequeued
 */
static uint16_t tx_ring_buffer_dequeue(uint8_t* buf, uint16_t max_len)
{
  uint16_t bytes_read = 0;
  uint16_t bytes_available;

  /* No critical section needed - single reader (main loop) */
  bytes_available = ring_buffer_used(g_tx_ring_buffer.head, g_tx_ring_buffer.tail, TX_RING_BUFFER_SIZE);

  if (bytes_available == 0) {
    return 0;
  }

  /* Read up to max_len bytes */
  bytes_read = (bytes_available < max_len) ? bytes_available : max_len;

  for (uint16_t i = 0; i < bytes_read; i++) {
    buf[i] = g_tx_ring_buffer.buffer[g_tx_ring_buffer.tail];
    g_tx_ring_buffer.tail = (g_tx_ring_buffer.tail + 1) & TX_RING_BUFFER_MASK;
  }

  return bytes_read;
}

/* RX Ring Buffer Functions (Single-writer, single-reader) */

/**
 * @brief Enqueue data to RX ring buffer (ISR context, no protection needed)
 * @param byte Byte to enqueue
 * @retval true if enqueued, false if buffer full
 */
static bool rx_ring_buffer_enqueue(uint8_t byte)
{
  /* No critical section needed - single writer (USB ISR) */
  if (ring_buffer_is_full(g_rx_ring_buffer.head, g_rx_ring_buffer.tail, RX_RING_BUFFER_SIZE)) {
    return false;
  }

  g_rx_ring_buffer.buffer[g_rx_ring_buffer.head] = byte;
  g_rx_ring_buffer.head = (g_rx_ring_buffer.head + 1) & RX_RING_BUFFER_MASK;

  return true;
}

/**
 * @brief Dequeue data from RX ring buffer (main loop context)
 * @param byte Pointer to store dequeued byte
 * @retval true if byte dequeued, false if buffer empty
 */
static bool rx_ring_buffer_dequeue(uint8_t* byte)
{
  /* No critical section needed - single reader (main loop) */
  if (ring_buffer_is_empty(g_rx_ring_buffer.head, g_rx_ring_buffer.tail)) {
    return false;
  }

  *byte = g_rx_ring_buffer.buffer[g_rx_ring_buffer.tail];
  g_rx_ring_buffer.tail = (g_rx_ring_buffer.tail + 1) & RX_RING_BUFFER_MASK;

  return true;
}

/* Public API Functions */

/**
 * @brief Enqueue data for USB CDC transmission (multi-writer safe)
 * @param data Data to transmit
 * @param len Length of data
 * @retval true if enqueued, false if buffer full
 */
bool usb_cdc_enqueue_tx(const uint8_t* data, uint16_t len)
{
  return tx_ring_buffer_enqueue(data, len);
}

/**
  * @brief  CDC_Itf_Init
  *         Initializes the CDC media low layer
  * @param  none
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Init(void)
{
  /* USER CODE BEGIN 3 */
  /* Set Application Buffers */
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);

  /* Initialize ring buffers */
  ring_buffer_init();

  return (USBD_OK);
  /* USER CODE END 3 */
}

/**
  * @brief  CDC_Itf_DeInit
  *         DeInitializes the CDC media low layer
  * @param  none
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_DeInit(void)
{
  /* USER CODE BEGIN 4 */
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  CDC_Itf_Control
  *         Manage the CDC class requests
  * @param  cmd: Command code
  * @param  pbuf: Buffer containing command data (request parameters)
  * @param  length: Number of data to be sent (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  /* USER CODE BEGIN 5 */
  USBD_SetupReqTypedef *req = (USBD_SetupReqTypedef *)pbuf;

  switch(cmd)
  {
  case CDC_SEND_ENCAPSULATED_COMMAND:

  break;

  case CDC_GET_ENCAPSULATED_RESPONSE:

  break;

  case CDC_SET_COMM_FEATURE:

  break;

  case CDC_GET_COMM_FEATURE:

  break;

  case CDC_CLEAR_COMM_FEATURE:

  break;

  /*******************************************************************************/
  /* Line Coding Structure                                                       */
  /*-----------------------------------------------------------------------------*/
  /* Offset | Field       | Size | Value  | Description                          */
  /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
  /* 4      | bCharFormat |   1  | Number | Stop bits                            */
  /*                                        0 - 1 Stop bit                       */
  /*                                        1 - 1.5 Stop bits                    */
  /*                                        2 - 2 Stop bits                      */
  /* 5      | bParityType |  1   | Number | Parity                               */
  /*                                        0 - None                              */
  /*                                        1 - Odd                               */
  /*                                        2 - Even                              */
  /*                                        3 - Mark                               */
  /*                                        4 - Space                              */
  /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
  /*******************************************************************************/
  case CDC_SET_LINE_CODING:
   LineCoding.bitrate = (uint32_t)(pbuf[0] | (pbuf[1] << 8) |
                            (pbuf[2] << 16) | (pbuf[3] << 24));
    LineCoding.format = pbuf[4];
    LineCoding.paritytype = pbuf[5];
    LineCoding.datatype = pbuf[6];
    break;

  case CDC_GET_LINE_CODING:
    pbuf[0] = (uint8_t)(LineCoding.bitrate);
    pbuf[1] = (uint8_t)(LineCoding.bitrate >> 8);
    pbuf[2] = (uint8_t)(LineCoding.bitrate >> 16);
    pbuf[3] = (uint8_t)(LineCoding.bitrate >> 24);
    pbuf[4] = LineCoding.format;
    pbuf[5] = LineCoding.paritytype;
    pbuf[6] = LineCoding.datatype;

    /* Add your code here */
    break;

  case CDC_SET_CONTROL_LINE_STATE:
	  break;

  case CDC_SEND_BREAK:
  break;

  default:
  break;
  }

  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  CDC_Itf_Receive
  *         Data received over USB OUT endpoint are sent over CDC interface
  *         through this function.
  * @param  Buf: Buffer of data to be transmitted
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Receive(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */
  uint32_t i;

  /* Enqueue received bytes to RX ring buffer (if USB input enabled) */
  if (uart_terminal_get_input_mode() == TERMINAL_INPUT_USB_ONLY ||
       uart_terminal_get_input_mode() == TERMINAL_INPUT_BOTH)
  {
    for (i = 0; i < *Len; i++)
    {
      rx_ring_buffer_enqueue(Buf[i]);
    }
  }

  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (USBD_OK);
  /* USER CODE END 6 */
}


/* USBD_CDC_ItfTypeDef structure for FS mode */
USBD_CDC_ItfTypeDef USBD_CDC_Interface_fops_FS =
{
  CDC_Itf_Init,
  CDC_Itf_DeInit,
  CDC_Itf_Control,
  CDC_Itf_Receive
};

/**
  * @brief  CDC_Transmit_FS
  *         Data to send over USB IN endpoint are sent over CDC interface
  *         through this function.
  * @param  Buf: Buffer of data to be sent
  * @param  Len: Number of data to be sent (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_BUSY or USBD_FAIL
  */
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 7 */
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pCDC;
  if (hcdc == NULL) {
    return USBD_FAIL;
  }
  if (hcdc->TxState != 0){
    return USBD_BUSY;
  }
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  /* USER CODE END 7 */
  return result;
}

/**
  * @brief  usb_cdc_process
  *         Process USB CDC RX and TX in main loop
  *         - Dequeue RX bytes and feed to terminal
  *         - Dequeue TX bytes and transmit via CDC
  * @retval None
  */
void usb_cdc_process(void)
{
  uint8_t byte;
  uint8_t tx_chunk[64];  /* USB FS max packet size */
  uint16_t tx_len;
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pCDC;

  /* USER CODE BEGIN usb_cdc_process */

  /* 1. Process RX: Dequeue bytes from RX ring buffer and feed to terminal */
  bool rx_processed = false;
  while (rx_ring_buffer_dequeue(&byte)) {
    uart_terminal_receive_byte(byte);
    rx_processed = true;
  }

  /* If no RX bytes processed, check for idle line */
  if (!rx_processed) {
    uart_terminal_process_idle();
  }

  /* 2. Process TX: Check if we can transmit and have data to send */
  if (hcdc != NULL && hcdc->TxState == 0 && !g_usb_tx_busy) {
    /* USB TX is free - dequeue up to 64 bytes and transmit */
    tx_len = tx_ring_buffer_dequeue(tx_chunk, 64);

    if (tx_len > 0) {
      /* Set flag to indicate TX in progress */
      g_usb_tx_busy = true;

      /* Transmit the chunk (non-blocking) */
      CDC_Transmit_FS(tx_chunk, tx_len);
      g_usb_tx_busy = false;
    }
  }

  /* USER CODE END usb_cdc_process */
}


/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
