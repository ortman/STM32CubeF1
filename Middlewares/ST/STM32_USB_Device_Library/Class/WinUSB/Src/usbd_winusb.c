/*
 * usbd_winusb.c
 *
 *  Created on: 29 ���. 2018 �.
 *      Author: ortman
 */

#include "usbd_winusb.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"

static uint8_t  USBD_WinUSB_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  USBD_WinUSB_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  USBD_WinUSB_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t  USBD_WinUSB_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_WinUSB_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  *USBD_WinUSB_GetCfgDesc(uint16_t *length);
static uint8_t  *USBD_WinUSB_GetDeviceQualifierDesc(uint16_t *length);

static USBD_WinUSB_EnpointsConfig_t* USBD_WinUSB_FindEpPos(uint8_t epAddr);

//static uint8_t  USBD_WinUSB_EP0_RxReady (USBD_HandleTypeDef  *pdev);

USBD_ClassTypeDef  USBD_WINUSB = {
  USBD_WinUSB_Init,
  USBD_WinUSB_DeInit,
  USBD_WinUSB_Setup,
  NULL, /*EP0_TxSent*/
  NULL, /*EP0_RxReady*/
  USBD_WinUSB_DataIn, /*DataIn*/
  USBD_WinUSB_DataOut, /*DataOut*/
  NULL, /*SOF */
  NULL,
  NULL,
  USBD_WinUSB_GetCfgDesc,
  USBD_WinUSB_GetCfgDesc,
  USBD_WinUSB_GetCfgDesc,
  USBD_WinUSB_GetDeviceQualifierDesc,
};

USBD_WinUSB_EnpointsConfig_t *endpointConf = 0;
USBD_WinUSB_VendorSetup_t vendorSetup = NULL;
uint8_t epCount = 0;


static uint8_t *USBD_WinUSB_CfgDesc = 0;

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_WinUSB_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC]  __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};

static uint8_t  USBD_WinUSB_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
	if (epCount == 0) {
		return 1;
	}

	pdev->pClassData = USBD_malloc(sizeof (USBD_WinUSB_HandleTypeDef));
	if (pdev->pClassData == NULL) {
	    return 1;
	}

	for (uint8_t i=0; i<epCount; ++i) {
	    USBD_LL_OpenEP(pdev,
	    		endpointConf[i].epAddr,
				endpointConf[i].epType,
				endpointConf[i].epSize);
	    endpointConf[i].state = USBD_OK;
	    if (((endpointConf[i].epAddr & 0x80) == 0) && (endpointConf[i].buff != NULL) && endpointConf[i].buffSize) {
	    	//USBD_DbgLog("Prepare receive 0x%02x size: %i\n", endpointConf[i].epAddr, endpointConf[i].buffSize);
	    	USBD_LL_PrepareReceive(pdev,
	    			endpointConf[i].epAddr,
					endpointConf[i].buff,
	    			endpointConf[i].buffSize);
	    }
	}

	return USBD_OK;
}

static uint8_t  USBD_WinUSB_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
  /* Close WinUSB EPs */
  for (uint8_t i=0; i<epCount; ++i) {
    USBD_LL_CloseEP(pdev, endpointConf[i].epAddr);
  }

  /* FRee allocated memory */
  if (pdev->pClassData != NULL) {
    USBD_free(pdev->pClassData);
    pdev->pClassData = NULL;
  }
//  if (USBD_WinUSB_CfgDesc != NULL) {
//    USBD_free(USBD_WinUSB_CfgDesc);
//    USBD_WinUSB_CfgDesc = NULL;
//  }
  return USBD_OK;
}

static uint8_t  USBD_WinUSB_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req) {
	//USBD_DbgLog("USBD_SMC_Setup bm:%i, b:%i\n", req->bmRequest, req->bRequest);
//	  uint16_t len = 0;
//	  uint8_t  *pbuf = NULL;
	  USBD_WinUSB_HandleTypeDef     *hSmc = (USBD_WinUSB_HandleTypeDef*) pdev->pClassData;

	  switch (req->bmRequest & USB_REQ_TYPE_MASK) {
	  	  case USB_REQ_TYPE_CLASS :
	  		  USBD_CtlError (pdev, req);
	  		  return USBD_FAIL;
	  		  break;

	  	  case USB_REQ_TYPE_STANDARD:
	  		  switch (req->bRequest) {
	  		  	  case USB_REQ_GET_INTERFACE :
	  		  		  USBD_CtlSendData (pdev, (uint8_t *)&hSmc->AltSetting, 1);
	  		  		  break;

	  		  	  case USB_REQ_SET_INTERFACE :
	  		  		  hSmc->AltSetting = (uint8_t)(req->wValue);
	  		  		  break;
	  		  }
	  		  break;
	  	  case USB_REQ_TYPE_VENDOR:
	  		  if (vendorSetup != NULL) {
	  			  return vendorSetup(pdev, req);
	  		  }
	  		  break;
	  }
	  return USBD_OK;
}

static uint8_t  USBD_WinUSB_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum) {
  /* Ensure that the FIFO is empty before a new transfer, this condition could
     be caused by  a new transfer before the end of the previous transfer */
  //USBD_DbgLog("USBD_SMC_DataIn %02x\n", epnum);
//  USBD_DbgLog("USBD_SMC_DataIn ep %02x: bmRq %02x, bRq %02x, wIdx %04x, wLen %04x, wVal %04x\n",
//		  epnum,
//		  pdev->request.bmRequest,
//		  pdev->request.bRequest,
//		  pdev->request.wIndex,
//		  pdev->request.wLength,
//		  pdev->request.wValue);
  USBD_WinUSB_EnpointsConfig_t *cfg = USBD_WinUSB_FindEpPos(epnum | 0x80);
  if (cfg != NULL) {
	  cfg->state = USBD_OK;
  }
  return USBD_OK;
}

static uint8_t  USBD_WinUSB_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum) {
//  USBD_DbgLog("USBD_SMC_DataOut %02x\n", epnum);
  USBD_WinUSB_EnpointsConfig_t *cfg = USBD_WinUSB_FindEpPos(epnum);
  if (cfg != NULL && cfg->buff != NULL) {
	  if (cfg->outEvent != NULL) {
		  if (cfg->outEvent(cfg->epAddr, cfg->buff, ((PCD_HandleTypeDef *)(pdev->pData))->OUT_ep[cfg->epAddr & 0x7F].xfer_len) == 0xee) {
			  return USBD_OK;
		  }
	  }
	  USBD_LL_PrepareReceive(pdev, cfg->epAddr, cfg->buff, cfg->buffSize);
  }
  return USBD_OK;
}

static uint8_t  *USBD_WinUSB_GetCfgDesc(uint16_t *length) {
  *length = 18 + epCount * 7;
  return USBD_WinUSB_CfgDesc;
}

static uint8_t  *USBD_WinUSB_GetDeviceQualifierDesc(uint16_t *length) {
  *length = sizeof (USBD_WinUSB_DeviceQualifierDesc);
  return USBD_WinUSB_DeviceQualifierDesc;
}

USBD_StatusTypeDef USBD_WinUSB_Config(USBD_WinUSB_EnpointsConfig_t *conf, uint8_t endpointsCount, USBD_WinUSB_VendorSetup_t setupCallback) {
	if (conf == 0 || endpointsCount == 0) {
		return USBD_FAIL;
	}
	endpointConf = conf;
	epCount = endpointsCount;
	vendorSetup = setupCallback;

	uint16_t descSize = 18 + epCount * 7;

	USBD_WinUSB_CfgDesc = USBD_malloc(descSize);
	if (USBD_WinUSB_CfgDesc == NULL) {
		return USBD_FAIL;
	}
	USBD_WinUSB_CfgDesc[0] = 0x09; /* bLength: Configuration Descriptor size */
	USBD_WinUSB_CfgDesc[1] = USB_DESC_TYPE_CONFIGURATION; /* bDescriptorType: Configuration */
	*(uint16_t*)(USBD_WinUSB_CfgDesc + 2) = descSize;
	USBD_WinUSB_CfgDesc[4] = 0x01; /*bNumInterfaces: 1 interface*/
	USBD_WinUSB_CfgDesc[5] = 0x01; /*bConfigurationValue: Configuration value*/
	USBD_WinUSB_CfgDesc[6] = 0x00; /*iConfiguration: Index of string descriptor describing
						the configuration*/
	USBD_WinUSB_CfgDesc[7] = 0xC0; /*bmAttributes: bus powered and Support Remote Wake-up */
	USBD_WinUSB_CfgDesc[8] = 0x32; /*MaxPower 100 mA: this current is used for detecting Vbus*/

	USBD_WinUSB_CfgDesc[9] = 0x09;  /*bLength: Interface Descriptor size*/
	USBD_WinUSB_CfgDesc[10] = USB_DESC_TYPE_INTERFACE; /*bDescriptorType: Interface descriptor type*/
	USBD_WinUSB_CfgDesc[11] = 0x00; /*bInterfaceNumber: Number of Interface*/
	USBD_WinUSB_CfgDesc[12] = 0x00; /*bAlternateSetting: Alternate setting*/
	USBD_WinUSB_CfgDesc[13] = epCount; /*bNumEndpoints*/
	USBD_WinUSB_CfgDesc[14] = 0xFF; /*bInterfaceClass: Vendor*/
	USBD_WinUSB_CfgDesc[15] = 0x00; /*bInterfaceSubClass : 1=BOOT, 0=no boot*/
	USBD_WinUSB_CfgDesc[16] = 0x00; /*nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse*/
	USBD_WinUSB_CfgDesc[17] = 0x02; /*iInterface: Index of string descriptor*/

	uint16_t idx = 18;
	for (uint8_t i=0; i<epCount; ++i) {
		USBD_WinUSB_CfgDesc[idx + 0] = 0x07; /*bLength: Endpoint Descriptor size*/
		USBD_WinUSB_CfgDesc[idx + 1] = USB_DESC_TYPE_ENDPOINT; /*bDescriptorType*/
		USBD_WinUSB_CfgDesc[idx + 2] = endpointConf[i].epAddr; /*bEndpointAddress*/
		USBD_WinUSB_CfgDesc[idx + 3] = endpointConf[i].epType; /*bmAttributes*/
		*(uint16_t*)(USBD_WinUSB_CfgDesc + idx + 4) = endpointConf[i].epSize; /*wMaxPacketSize*/
		USBD_WinUSB_CfgDesc[idx + 6] = endpointConf[i].epInterval; /*bInterval*/
		idx += 7;
	}
	return USBD_OK;
}

static USBD_WinUSB_EnpointsConfig_t* USBD_WinUSB_FindEpPos(uint8_t epAddr) {
	for (uint8_t i = 0; i < epCount; ++i) {
		if (endpointConf[i].epAddr == epAddr) {
			return endpointConf + i;
		}
	}
	return NULL;
}

USBD_StatusTypeDef USBD_WinUSB_getStateSend(uint8_t epAddr) {
  USBD_WinUSB_EnpointsConfig_t *conf = USBD_WinUSB_FindEpPos(epAddr);
  if (conf == NULL) {
	  return USBD_FAIL;
  }
  return conf->state;
}

USBD_StatusTypeDef USBD_WinUSB_Send(USBD_HandleTypeDef  *pdev, uint8_t epAddr, uint8_t *buff, uint16_t len) {
	  if ((pdev->dev_state != USBD_STATE_CONFIGURED) || ((epAddr & 0x80) == 0x00) ) {
		  return USBD_FAIL;
	  }
	  USBD_WinUSB_EnpointsConfig_t *conf = USBD_WinUSB_FindEpPos(epAddr);
	  if (conf == NULL) {
		  return USBD_FAIL;
	  }
	  if (conf->state != USBD_OK) {
		  return USBD_BUSY;
	  }
	  conf->state = USBD_BUSY;
	  //USBD_DbgLog("transmit %02x, %i, %i\n", epAddr, len, buff);
	  return USBD_LL_Transmit(pdev, epAddr, buff, len);
}
