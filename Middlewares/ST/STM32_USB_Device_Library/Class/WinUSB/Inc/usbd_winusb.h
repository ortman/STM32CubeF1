/*
 * usbd_winusb.h
 *
 *  Created on: 29 ���. 2018 �.
 *      Author: ortman
 */

#ifndef __USBD_WINUSB_H_
#define __USBD_WINUSB_H_

#include  "usbd_ioreq.h"

typedef struct {
	uint8_t epType;
	uint8_t epAddr;
	uint16_t epSize;
	uint8_t epInterval;
	uint8_t  (*outEvent) (uint8_t epAddr, uint8_t *buff, uint16_t buffSize);
	USBD_StatusTypeDef state;
	uint8_t *buff;
	uint32_t buffSize;
} USBD_WinUSB_EnpointsConfig_t;

typedef uint8_t  (*USBD_WinUSB_VendorSetup_t) (struct _USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef  *req);

typedef struct
{
  uint32_t             Protocol;
  uint32_t             IdleState;
  uint32_t             AltSetting;
}
USBD_WinUSB_HandleTypeDef;

extern USBD_ClassTypeDef  USBD_WINUSB;
#define USBD_WINUSB_CLASS    &USBD_WINUSB

USBD_StatusTypeDef USBD_WinUSB_Config(USBD_WinUSB_EnpointsConfig_t *conf, uint8_t endpointsCount, USBD_WinUSB_VendorSetup_t setupCallback);
USBD_StatusTypeDef USBD_WinUSB_getStateSend(uint8_t epAddr);
USBD_StatusTypeDef USBD_WinUSB_Send(USBD_HandleTypeDef  *pdev, uint8_t epAddr, uint8_t *buff, uint16_t len);

#endif /* __USBD_WINUSB_H_ */
