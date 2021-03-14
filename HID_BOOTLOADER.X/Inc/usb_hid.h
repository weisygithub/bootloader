#ifndef USB_HID_H
#define	USB_HID_H

#include "system.h"

#define USBSRAMSPACE 0x00

bool USBDataReady(void);
uint8_t HIDRxGetCount(void);
void HIDGetBlock(uint8_t *buffer,uint32_t size);
uint8_t *USBGet(uint8_t *buffer,uint32_t size);

void USBPutCont(uint8_t *buffer,uint32_t size);
#endif