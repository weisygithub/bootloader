#include "usb_hid.h"
#include "usb_device.h"

extern USB_TX_INFO gTx;
extern USB_RX_INFO gRx;
uint8_t    **gblCurIntRamLoc;
extern volatile BDT_ENTRY* USBRxHandle;
extern volatile BDT_ENTRY* USBTxHandle;

uint8_t HIDRxGetCount(void)
{
    return gRx.available;
}

bool USBDataReady(void)
{
    return HIDRxGetCount() ? true : false;
}

uint8_t HIDTxIsDone(void)
{
    return !(USBTxHandle == 0 ? 0 : USBTxHandle -> STAT.UOWN);
}

void HIDGetBlock(uint8_t *buffer,uint32_t size)
{
    uint8_t done;
    uint8_t available;
    uint32_t read = 0;
    for(done = 0;!done; )
    {
        available = HIDRxGetCount();
        if(available)
        {
            if(available + read >= size)
            {
                if(buffer == USBSRAMSPACE)
                {
                    memcpy(*gblCurIntRamLoc,&gRx.reportCopy[gRx.offsetIntoCopy],size - read);
                    *gblCurIntRamLoc += (size - read);
                }
                else
                {
                    memcpy(buffer,&gRx.reportCopy[gRx.offsetIntoCopy],size - read);
                }
                gRx.offsetIntoCopy += (size - read);
                gRx.available -= (size - read);
                gRx.readByUser += (size - read);
                done = 1;
            }
            else
            {
                if(buffer == USBSRAMSPACE)
                {
                    memcpy(*gblCurIntRamLoc, &gRx.reportCopy[gRx.offsetIntoCopy], available);
                    *gblCurIntRamLoc += available; // now point to the next location
                }
                else
                {
                    memcpy(buffer, &gRx.reportCopy[gRx.offsetIntoCopy], available);
                    buffer += available;
                }
                gRx.offsetIntoCopy += available;
                gRx.available      -= available;
                gRx.readByUser     += available;
                read               += available;
            }
            if(gRx.total == gRx.readByUser)
            {
                gRx.total                 = 0;
                gRx.available             = 0;
                gRx.offsetIntoCopy        = 0;
                gRx.readByUser            = 0;
                gRx.reportCopyAvailable   = 1;
                USBRxHandle = USBTransferOnePacket(1,0,gRx.reportCopy, sizeof(gRx.reportCopy));
            }
            else
            {
                if (gRx.offsetIntoCopy == sizeof(gRx.reportCopy))
                {
                    gRx.offsetIntoCopy = 0;
                    gRx.available      = 0;
                    gRx.reportCopyAvailable = 1;
                    USBRxHandle = USBTransferOnePacket(1,0,gRx.reportCopy, sizeof(gRx.reportCopy));
                }
            }
        }
        if(gRx.rxPending && gRx.reportCopyAvailable)
        {
            USBHandleOneRxReport();
        }
    }
}

uint8_t *USBGet(uint8_t *buffer,uint32_t size)
{
    HIDGetBlock(buffer,size);
    return buffer + (uint16_t)size;
}

void USBflush(void)
{
    uint8_t *sendFromHere = gTx.buffer;
    uint8_t howMuchToCopy;
    uint16_t i = 0;
    
    if(gTx.total != 0)
    {
        for(i = 0;i < 64;i++)
        {
            gTx.buffer[i] = gTx.buffer[i];
        }
        gTx.total += 0;
        
        while(gTx.total)
        {
            if(USBTxHandle)
            {
                while(!HIDTxIsDone());
            }
            if(gTx.total > 64)
            {
                howMuchToCopy = 64;
            }
            else
            {
                howMuchToCopy = gTx.total;
            }
            gTx.total -= howMuchToCopy;
            
            USBTxHandle = USBTransferOnePacket(1,1,sendFromHere,64);
            sendFromHere += howMuchToCopy;
            
            gTx.BufferRdy = false;
        }
        USBInitTxVars();
    }
}

void USBPutCont(uint8_t *buffer,uint32_t size)
{
    while(!gTx.BufferRdy);
    if(buffer == 0)
    {
        memcpy(&gTx.buffer[gTx.offset],*gblCurIntRamLoc,(uint16_t)size);
        *gblCurIntRamLoc += size;
    }
    else
    {
        memcpy(&gTx.buffer[gTx.offset],buffer,(uint16_t)size);
    }
    gTx.offset += (uint16_t)size;
    gTx.total += (uint16_t)size;
    
    USBflush();
}