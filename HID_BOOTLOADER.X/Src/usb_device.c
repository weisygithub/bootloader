#include "usb_device.h"

uint8_t idle_rate;
uint8_t active_protocol;
uint8_t USBDeviceState;
uint8_t USBActiveConfiguration;
uint8_t USBAlternateInterface[1];
volatile BDT_ENTRY *pBDTEntryEP0OutCurrent;
volatile BDT_ENTRY *pBDTEntryEP0OutNext;
volatile BDT_ENTRY *pBDTEntryOut[1+1];
volatile BDT_ENTRY *pBDTEntryIn[1+1];
uint8_t shortPacketStatus;
uint8_t controlTransferState;
IN_PIPE inPipes[1];
OUT_PIPE outPipes[1];
uint8_t *pDst;
bool RemoteWakeup;
uint8_t USTATcopy;
uint8_t *USBInData[1];

volatile BDT_ENTRY BDT[(1 + 1) * 4] __attribute__ ((aligned (512)));
volatile CTRL_TRF_SETUP SetupPkt;
volatile uint8_t CtrlTrfData[8];

USB_RX_INFO gRx;
USB_TX_INFO gTx;
volatile BDT_ENTRY* USBRxHandle = NULL;
volatile BDT_ENTRY* USBTxHandle = NULL;

extern const USB_DEVICE_DESCRIPTOR device_dsc;
extern const uint8_t configDescriptor1[];
extern const uint8_t *const USB_CD_Ptr[];
extern const uint8_t *const USB_SD_Ptr[];

#if !defined(__USB_DESCRIPTORS_C)
    extern const struct{uint8_t report[29];} hid_rpt01;
#endif

void USBClearInterruptFlag(uint8_t* reg, uint8_t flag)
{
    *reg = (0x01<<flag);        
}

void USBInitRxVars(void)
{
    gRx.waitingForFirstReport = 1;
    gRx.total                 = 0;
    gRx.available             = 0;
    gRx.offsetIntoCopy        = 0;
    gRx.rxPending             = 0;
    gRx.reportCopyAvailable   = 1;
    gRx.readByUser            = 0;
}
void USBInitTxVars(void)
{
    gTx.total = 0;
    gTx.offset = 0; // skip the length
}

void USBDeviceInit(void)
{
    uint8_t i = 0;
    U1EIR = 0xFF;
    U1IR = 0xFF;                    
    U1EIE = 0x9F;
    U1IE = 0x9F;
    U1PWRCbits.USBPWR = 1;
    U1BDTP1 = (((uint16_t)BDT)/256);
    U1CONbits.PPBRST = 1;                    
    U1CONbits.PPBRST = 0;
    U1ADDR = 0x00;                   
    memset((void *)&U1EP1,0x00,0);
    for(i = 0;i < (sizeof(BDT) / sizeof(BDT_ENTRY));i++)
    {
        BDT[i].Val = 0x00;
    }
    U1EP0 = EP_CTRL | USB_HANDSHAKE_ENABLED;        
    while(U1IRbits.TRNIF == 1)      
    {
        USBClearInterruptFlag((uint8_t*)&U1IR,3);
    }
    inPipes[0].info.Val = 0;
    outPipes[0].info.Val = 0;
    outPipes[0].wCount.Val = 0;
    U1CONbits.PKTDIS = 0;           
    pBDTEntryIn[0] = (volatile BDT_ENTRY*)&BDT[EP0_IN_EVEN];
    USBActiveConfiguration = 0;      
    USBDeviceState = DETACHED_STATE;
}

void USBInitialize(void)
{
    memset((uint8_t *)&gTx,0x0,sizeof(gTx));
    USBInitRxVars();
    USBInitTxVars();
    USBDeviceInit();
    gTx.BufferRdy=1;
}

void USBConfigureEndpoint(uint8_t EPNum, uint8_t direction)
{
    volatile BDT_ENTRY* handle;
    handle = (volatile BDT_ENTRY*)&BDT[EP0_OUT_EVEN];
    handle += ((4 * (4 * EPNum + 2 * direction)) / sizeof(BDT_ENTRY));
    handle->STAT.UOWN = 0;
    if(direction == 0)
    {
        pBDTEntryOut[EPNum] = handle;
    }
    else
    {
        pBDTEntryIn[EPNum] = handle;
    }
    handle->STAT.DTS = 0;
    (handle + 1)->STAT.DTS = 1;
}

void USBEnableEndpoint(uint8_t ep, uint8_t options)
{
    uint16_t* p;
    p = (uint16_t*)(&U1EP0 + ep);
    *p = options;
        
    if(options & USB_OUT_ENABLED)
    {
        USBConfigureEndpoint(ep,0);
    }
    if(options & USB_IN_ENABLED)
    {
        USBConfigureEndpoint(ep,1);
    }
}

volatile BDT_ENTRY* USBTransferOnePacket(uint8_t ep,uint8_t dir,uint8_t* data,uint8_t len)
{
    volatile BDT_ENTRY* handle;
    if(dir != 0)
    {
        handle = pBDTEntryIn[ep];
    }
    else
    {
        handle = pBDTEntryOut[ep];
    }
    handle->ADR = data;
    handle->CNT = len;
    handle->STAT.Val &= _DTSMASK;
    handle->STAT.Val |= _USIE | _DTSEN;
    if(dir != 0)
    {
        ((BYTE_VAL*)&pBDTEntryIn[ep])->Val ^= USB_NEXT_PING_PONG;
    }
    else
    {
        ((BYTE_VAL*)&pBDTEntryOut[ep])->Val ^= USB_NEXT_PING_PONG;
    }
    return handle;
}

void USBStdFeatureReqHandler(void)
{
    BDT_ENTRY *p;
    uint16_t* pUEP;             
    if((SetupPkt.bFeature == DEVICE_REMOTE_WAKEUP) && (SetupPkt.Recipient == RCPT_DEV))
    {
        inPipes[0].info.bits.busy = 1;
        if(SetupPkt.bRequest == SET_FEATURE)
            RemoteWakeup = 1;
        else
            RemoteWakeup = 0;
    }
    if((SetupPkt.bFeature == ENDPOINT_HALT) && (SetupPkt.Recipient == RCPT_EP) && (SetupPkt.EPNum != 0))
    {
        uint8_t i;
        inPipes[0].info.bits.busy = 1;
        for(i = 0; i < 2; i++)
        {
            p = (BDT_ENTRY*)&BDT[EP0_OUT_EVEN];
            p += (4 * SetupPkt.EPNum + 2 * SetupPkt.EPDir + i);
            if(SetupPkt.bRequest == SET_FEATURE)
            {
                p->STAT.Val = _USIE|_BSTALL;
            }
            else
            {
                pUEP = (unsigned int*)(&U1EP0+SetupPkt.EPNum);
                *pUEP &= ~UEP_STALL;
                if(SetupPkt.EPDir == 1) // IN
                {
                    p->STAT.Val = _UCPU|_DAT1;
                }
                else
                {
                    p->STAT.Val = _USIE|_DAT1|_DTSEN;
                }
            }
        }
    }
}

void USBStdGetDscHandler(void)
{
    if(SetupPkt.bmRequestType == 0x80)
    {
        inPipes[0].info.Val = USB_INPIPES_ROM | USB_INPIPES_BUSY | USB_INPIPES_INCLUDE_ZERO;
        switch(SetupPkt.bDescriptorType)
        {
            case 0x01://USB_DESCRIPTOR_DEVICE
                inPipes[0].pSrc.bRom = (const uint8_t *)&device_dsc;
                inPipes[0].wCount.Val = sizeof(device_dsc);
                break;
                
            case 0x02://USB_DESCRIPTOR_CONFIGURATION:
                inPipes[0].pSrc.bRom = *(USB_CD_Ptr+SetupPkt.bDscIndex);
                inPipes[0].wCount.Val = *(inPipes[0].pSrc.wRom+1);                // Set data count
                break;

            case 0x03://USB_DESCRIPTOR_STRING:
                if (SetupPkt.bDscIndex == 3)
                {
//                    inPipes[0].info.Val &= ~USB_INPIPES_ROM;
//                    inPipes[0].info.Val |=  USB_INPIPES_RAM;
//                    inPipes[0].pSrc.bRam = (uint8_t *)&gSerialNumberDescriptor;
//                    inPipes[0].wCount.Val = *inPipes[0].pSrc.bRam;                    // Set data count
                }
                else
                {
                    inPipes[0].pSrc.bRom = *(USB_SD_Ptr+SetupPkt.bDscIndex);
                    inPipes[0].wCount.Val = *inPipes[0].pSrc.bRom;                    // Set data count
                }
                break;
            default:
                inPipes[0].info.Val = 0;
                break;
        }
    }
}

void USBStdGetStatusHandler(void)
{
    CtrlTrfData[0] = 0;
    CtrlTrfData[1] = 0;
    switch(SetupPkt.Recipient)
    {
        case RCPT_DEV:
            inPipes[0].info.bits.busy = 1;
            if(RemoteWakeup == 1)CtrlTrfData[0]|=0x02;
            break;
        case RCPT_INTF:
            inPipes[0].info.bits.busy = 1;     // No data to update
            break;
        case RCPT_EP:
            inPipes[0].info.bits.busy = 1;
            BDT_ENTRY *p;
            p = (BDT_ENTRY*)&BDT[EP0_OUT_EVEN];
            p += (4 * SetupPkt.EPNum + 2 * SetupPkt.EPDir);
            if(p->STAT.Val & _BSTALL)CtrlTrfData[0]=0x01;
            break;

    }
    if(inPipes[0].info.bits.busy == 1)
    {
        inPipes[0].pSrc.bRam = (uint8_t*)&CtrlTrfData;            // Set Source
        inPipes[0].info.bits.ctrl_trf_mem = USB_INPIPES_RAM;               // Set memory type
        inPipes[0].wCount.v[0] = 2;                         // Set data count
    }
}

void USBStdSetCfgHandler(void)
{
    inPipes[0].info.bits.busy = 1;            
    memset((void*)&U1EP1,0x00,0);
    memset((void*)&USBAlternateInterface,0x00,1);
    USBActiveConfiguration = SetupPkt.bConfigurationValue;
    if(SetupPkt.bConfigurationValue == 0)
    {
        USBDeviceState = ADDRESS_STATE;
    }
    else
    {
        USBDeviceState = CONFIGURED_STATE;
        USBInitEP((uint8_t const*)(USB_CD_Ptr[USBActiveConfiguration-1]));
        USBEnableEndpoint(1,USB_IN_ENABLED | USB_OUT_ENABLED | USB_HANDSHAKE_ENABLED | USB_DISALLOW_SETUP);
        USBRxHandle = USBTransferOnePacket(1,0,gRx.reportCopy, sizeof(gRx.reportCopy));
        USBTxHandle = NULL;
    }
}

void USBCheckStdRequest(void)
{
    if(SetupPkt.RequestType != STANDARD) return;
    switch(SetupPkt.bRequest)
    {
        case SET_ADR:
            inPipes[0].info.bits.busy = 1;
            USBDeviceState = ADR_PENDING_STATE;
            break;
        case GET_DSC:
            USBStdGetDscHandler();
            break;
        case SET_CFG:
            USBStdSetCfgHandler();
            break;
        case GET_CFG:
            inPipes[0].pSrc.bRam = (uint8_t*)&USBActiveConfiguration;         // Set Source
            inPipes[0].info.bits.ctrl_trf_mem = USB_INPIPES_RAM;               // Set memory type
            inPipes[0].wCount.v[0] = 1;                         // Set data count
            inPipes[0].info.bits.busy = 1;
            break;
        case GET_STATUS:
            USBStdGetStatusHandler();
            break;
        case CLR_FEATURE:
        case SET_FEATURE:
            USBStdFeatureReqHandler();
            break;
        case GET_INTF:
            inPipes[0].pSrc.bRam = (uint8_t*)&USBAlternateInterface+SetupPkt.bIntfID;
            inPipes[0].info.bits.ctrl_trf_mem = USB_INPIPES_RAM;
            inPipes[0].wCount.v[0] = 1;
            inPipes[0].info.bits.busy = 1;
            break;
        case SET_INTF:
            inPipes[0].info.bits.busy = 1;
            USBAlternateInterface[SetupPkt.bIntfID] = SetupPkt.bAltID;
            break;
        case SET_DSC:
        case SYNCH_FRAME:
        default:
            break;
    }
}

void USBCheckHIDRequest(void)
{
    if(SetupPkt.Recipient != RCPT_INTF) return;
    if(SetupPkt.bIntfID != 0x00) return;
    if(SetupPkt.bRequest == GET_DSC)
    {
        switch(SetupPkt.bDescriptorType)
        {
            case 0x21://DSC_HID:           
                if(USBActiveConfiguration == 1)
                {
                    inPipes[0].pSrc.bRom = (const uint8_t*)&configDescriptor1 + 18;
                    inPipes[0].wCount.Val = 9;
                    inPipes[0].info.Val = USB_INPIPES_INCLUDE_ZERO | USB_INPIPES_ROM | USB_INPIPES_BUSY;
                }
                break;
            case 0x22://DSC_RPT:             
                if(USBActiveConfiguration == 1)
                {
                    inPipes[0].pSrc.bRom = (const uint8_t*)&hid_rpt01;
                    inPipes[0].wCount.Val = 29;
                    inPipes[0].info.Val = USB_INPIPES_INCLUDE_ZERO | USB_INPIPES_ROM | USB_INPIPES_BUSY;                    
                }
                break;
            case 0x23://DSC_PHY:
                inPipes[0].info.Val = USB_INPIPES_NO_DATA | USB_INPIPES_BUSY;
                break;
        }
    }
    if(SetupPkt.RequestType != CLASS) return;
    switch(SetupPkt.bRequest)
    {
        case 0x01://GET_REPORT:
            break;
        case 0x09://SET_REPORT:      
            break;
        case 0x02://GET_IDLE:
            inPipes[0].pSrc.bRam = (uint8_t*)&idle_rate;
            inPipes[0].wCount.Val = 1;
            inPipes[0].info.Val = USB_INPIPES_INCLUDE_ZERO | USB_INPIPES_RAM | USB_INPIPES_BUSY;
            break;
        case 0x0A://SET_IDLE:
            inPipes[0].info.Val = USB_INPIPES_NO_DATA | USB_INPIPES_BUSY;
            idle_rate = SetupPkt.W_Value.val_byte.HB;
            break;
        case 0x03://GET_PROTOCOL:
            inPipes[0].pSrc.bRam = (uint8_t*)&active_protocol;
            inPipes[0].wCount.Val = 1;
            inPipes[0].info.Val = USB_INPIPES_NO_OPTIONS | USB_INPIPES_RAM | USB_INPIPES_BUSY;
            break;
        case 0x0B://SET_PROTOCOL:
            inPipes[0].info.Val = USB_INPIPES_NO_DATA | USB_INPIPES_BUSY;
            active_protocol = SetupPkt.W_Value.val_byte.LB;
            break;
    }
}

void USBCtrlTrfTxService(void)
{
    WORD_VAL byteToSend;
    if(inPipes[0].wCount.Val < 8)
    {
        byteToSend.Val = inPipes[0].wCount.Val;
        if(shortPacketStatus == SHORT_PKT_NOT_USED)
        {
            shortPacketStatus = SHORT_PKT_PENDING;
        }
        else if(shortPacketStatus == SHORT_PKT_PENDING)
        {
            shortPacketStatus = SHORT_PKT_SENT;
        }
    }
    else
    {
        byteToSend.Val = 8;
    }
    pBDTEntryIn[0]->STAT.Val |= byteToSend.val_byte.HB;
    pBDTEntryIn[0]->CNT = byteToSend.val_byte.LB;
    inPipes[0].wCount.Val = inPipes[0].wCount.Val - byteToSend.Val;
    pDst = (uint8_t*)CtrlTrfData;
    if(inPipes[0].info.bits.ctrl_trf_mem == USB_INPIPES_ROM)
    {
        while(byteToSend.Val)
        {
            *pDst++ = *inPipes[0].pSrc.bRom++;
            byteToSend.Val--;
        }
    }
    else
    {
        while(byteToSend.Val)
        {
            *pDst++ = *inPipes[0].pSrc.bRam++;
            byteToSend.Val--;
        }
    }
}

void USBCtrlEPServiceComplete(void)
{
    U1CONbits.PKTDIS = 0;
    if(inPipes[0].info.bits.busy == 0)
    {
        if(outPipes[0].info.bits.busy == 1)
        {
            controlTransferState = CTRL_TRF_RX;
            pBDTEntryIn[0]->CNT = 0;
            pBDTEntryIn[0]->STAT.Val = _USIE|_DAT1|_DTSEN;
            
            pBDTEntryEP0OutNext->CNT = 8;
            pBDTEntryEP0OutNext->ADR = (uint8_t*)&CtrlTrfData;
            pBDTEntryEP0OutNext->STAT.Val = _USIE|_DAT1|_DTSEN;
        }
        else
        {
            pBDTEntryEP0OutNext->CNT = 8;
            pBDTEntryEP0OutNext->ADR = (uint8_t*)&SetupPkt;
            pBDTEntryEP0OutNext->STAT.Val = _USIE|_DAT0|_DTSEN|_BSTALL;
            pBDTEntryIn[0]->STAT.Val = _USIE|_BSTALL; 
        }
    }
    else
    {
        if(outPipes[0].info.bits.busy == 0)
        {
            if(SetupPkt.DataDir == DEV_TO_HOST)
            {
                if(SetupPkt.wLength < inPipes[0].wCount.Val)
                {
                    inPipes[0].wCount.Val = SetupPkt.wLength;
                }
                USBCtrlTrfTxService();
                controlTransferState = CTRL_TRF_TX;
                pBDTEntryEP0OutNext->CNT = 8;
                pBDTEntryEP0OutNext->ADR = (uint8_t*)&SetupPkt;
                pBDTEntryEP0OutNext->STAT.Val = _USIE;           // Note: DTSEN is 0!

                pBDTEntryEP0OutCurrent->CNT = 8;
                pBDTEntryEP0OutCurrent->ADR = (uint8_t*)&SetupPkt;
                pBDTEntryEP0OutCurrent->STAT.Val = _USIE;           // Note: DTSEN is 0!

                pBDTEntryIn[0]->ADR = (uint8_t*)&CtrlTrfData;
                pBDTEntryIn[0]->STAT.Val = _USIE|_DAT1|_DTSEN;
            }
            else
            {
                controlTransferState = CTRL_TRF_RX;
                pBDTEntryIn[0]->CNT = 0;
                pBDTEntryIn[0]->STAT.Val = _USIE|_DAT1|_DTSEN;

                pBDTEntryEP0OutNext->CNT = 8;
                pBDTEntryEP0OutNext->ADR = (uint8_t*)&CtrlTrfData;
                pBDTEntryEP0OutNext->STAT.Val = _USIE|_DAT1|_DTSEN;
            }
        }
    }
}

void USBCtrlTrfSetupHandler(void)
{
    if(pBDTEntryIn[0]->STAT.UOWN != 0)
    {
        pBDTEntryIn[0]->STAT.Val = _UCPU;           
    }
    shortPacketStatus = SHORT_PKT_NOT_USED;
    /* Stage 1 */
    controlTransferState = WAIT_SETUP;
    inPipes[0].wCount.Val = 0;
    inPipes[0].info.Val = 0;
    /* Stage 2 */
    USBCheckStdRequest();
    USBCheckHIDRequest();
    /* Stage 3 */
    USBCtrlEPServiceComplete();
}

void USBCtrlTrfRxService(void)
{
    uint8_t byteToRead;
    uint8_t i;
    byteToRead = pBDTEntryEP0OutCurrent->CNT;
    if(byteToRead > outPipes[0].wCount.Val)
    {
        byteToRead = outPipes[0].wCount.Val;
    }
    else
    {
        outPipes[0].wCount.Val = outPipes[0].wCount.Val - byteToRead;
    }
    for(i = 0;i < byteToRead;i++)
    {
        *outPipes[0].pDst.bRam++ = CtrlTrfData[i];
    }
    if(outPipes[0].wCount.Val > 0)
    {
        pBDTEntryEP0OutNext->CNT = 8;
        pBDTEntryEP0OutNext->ADR = (uint8_t*)&CtrlTrfData;
        if(pBDTEntryEP0OutCurrent->STAT.DTS == 0)
        {
            pBDTEntryEP0OutNext->STAT.Val = _USIE|_DAT1|_DTSEN;
        }
        else
        {
            pBDTEntryEP0OutNext->STAT.Val = _USIE|_DAT0|_DTSEN;
        }
    }
    else
    {
        pBDTEntryEP0OutNext->ADR = (uint8_t*)&SetupPkt;
        if(outPipes[0].pFunc != NULL)
        {
            outPipes[0].pFunc();
        }
        outPipes[0].info.bits.busy = 0;
    }
}

void USBPrepareForNextSetupTrf(void)
{
    if((controlTransferState == CTRL_TRF_RX) &&
       (U1CONbits.PKTDIS == 1) &&
       (pBDTEntryEP0OutCurrent->CNT == sizeof(CTRL_TRF_SETUP)) &&
       (pBDTEntryEP0OutCurrent->STAT.PID == SETUP_TOKEN) &&
       (pBDTEntryEP0OutNext->STAT.UOWN == 0))
    {
        uint8_t setup_cnt;
        pBDTEntryEP0OutNext->ADR = (uint8_t*)&SetupPkt;
        for(setup_cnt = 0; setup_cnt < sizeof(CTRL_TRF_SETUP); setup_cnt++)
        {
            *(((uint8_t*)&SetupPkt)+setup_cnt) = *(((uint8_t*)&CtrlTrfData)+setup_cnt);
        }
    }
    else
    {
        controlTransferState = WAIT_SETUP;
        pBDTEntryEP0OutNext->CNT = 8;
        pBDTEntryEP0OutNext->ADR = (uint8_t*)&SetupPkt;
        pBDTEntryEP0OutNext->STAT.Val = _USIE|_DAT0|_DTSEN|_BSTALL;
        pBDTEntryIn[0]->STAT.Val = _UCPU;
        BDT_ENTRY* p;
        p = (BDT_ENTRY*)(((uint16_t)pBDTEntryIn[0])^USB_NEXT_EP0_IN_PING_PONG);
        p->STAT.Val = _UCPU;
    }
    if(outPipes[0].info.bits.busy == 1)
    {
        if(outPipes[0].pFunc != NULL)
        {
            outPipes[0].pFunc();
        }
        outPipes[0].info.bits.busy = 0;
    }
}

void USBCtrlTrfOutHandler(void)
{
    if(controlTransferState == CTRL_TRF_RX)
    {
        USBCtrlTrfRxService();
    }
    else
    {
        USBPrepareForNextSetupTrf();
    }
}

void USBCtrlTrfInHandler(void)
{
    uint8_t lastDTS;
    lastDTS = pBDTEntryIn[0]->STAT.DTS;
    ((BYTE_VAL*)&pBDTEntryIn[0])->Val ^= USB_NEXT_EP0_IN_PING_PONG;
    if(USBDeviceState == ADR_PENDING_STATE)
    {
        U1ADDR = SetupPkt.bDevADR.Val;
        if(U1ADDR > 0)
        {
            USBDeviceState = ADDRESS_STATE;
        }
        else
        {
            USBDeviceState = DEFAULT_STATE;
        }
    }
    if(controlTransferState == CTRL_TRF_TX)
    {
        pBDTEntryIn[0]->ADR = (uint8_t *)CtrlTrfData;
        USBCtrlTrfTxService();
        if(shortPacketStatus == SHORT_PKT_SENT)
        {
            pBDTEntryIn[0]->STAT.Val = _USIE|_BSTALL;
        }
        else
        {
            if(lastDTS == 0)
            {
                pBDTEntryIn[0]->STAT.Val = _USIE|_DAT1|_DTSEN;
            }
            else
            {
                pBDTEntryIn[0]->STAT.Val = _USIE|_DAT0|_DTSEN;
            }
        }
    }
    else
    {
        USBPrepareForNextSetupTrf();
    }
}

void USBCtrlEPService(void)
{
    if((USTATcopy & USTAT_EP0_PP_MASK) == USTAT_EP0_OUT_EVEN)
    {
        pBDTEntryEP0OutCurrent = (volatile BDT_ENTRY*)&BDT[(USTATcopy & USTAT_EP_MASK)>>2];
        pBDTEntryEP0OutNext = pBDTEntryEP0OutCurrent;
        ((BYTE_VAL*)&pBDTEntryEP0OutNext)->Val ^= USB_NEXT_EP0_OUT_PING_PONG;
        if(pBDTEntryEP0OutCurrent->STAT.PID == SETUP_TOKEN)
        {
            USBCtrlTrfSetupHandler();
        }
        else
        {
            USBCtrlTrfOutHandler();
        }
    }
    else if((USTATcopy & USTAT_EP0_PP_MASK) == USTAT_EP0_IN)
    {
        USBCtrlTrfInHandler();
    }
}

void USBHandleOneRxReport(void)
{
    uint16_t i = 0;
    if (gRx.waitingForFirstReport)
    {
        gRx.total = 64;
        i = 64;
        while(i--)
        {
            gRx.reportCopy[i] = gRx.reportCopy[i];
        }
        gRx.offsetIntoCopy = 0;
        if (gRx.total > 64)
        {
            gRx.available = 64;
            gRx.waitingForFirstReport = 0;
        }
        else
        {
            gRx.available = gRx.total;
            gRx.waitingForFirstReport = 1;
        }
    }
    else
    {
        gRx.offsetIntoCopy = 0;
        if (gRx.readByUser + sizeof(gRx.reportCopy) >= gRx.total)
        {
            gRx.available = gRx.total - gRx.readByUser;
            gRx.waitingForFirstReport = 1;
        } 
        else
        {
            gRx.available = sizeof(gRx.reportCopy);
            gRx.waitingForFirstReport = 0;
        }
    }
}

void USBEPService(void)
{
    if((USTATcopy & USTAT_EP1_PP_MASK) == USTAT_EP1_IN)
    {
        gTx.FastPutInProgress=0;
        if(!gTx.SENDSSS)    
            gTx.BufferRdy = 1;
    }
    else if((USTATcopy & USTAT_EP1_PP_MASK) == USTAT_EP1_OUT_EVEN)
    {
        if (gRx.reportCopyAvailable)
        {
            USBHandleOneRxReport();            
        }    
        else   
        {
            gRx.rxPending = 1;
        }
    }
}

void USBDeviceTasks(void)
{
    uint8_t i = 0;
    if(USBDeviceState == DETACHED_STATE)
    {
        U1CON = 0;
        U1IE = 0;                                
        while(!U1CONbits.USBEN)
        {
            U1CONbits.USBEN = 1;
        }
        USBDeviceState = ATTACHED_STATE;
        U1CNFG1 = 0x02;
        U1CNFG2 = 0x10;
    }
    if(USBDeviceState == ATTACHED_STATE)
    {
        U1IR = 0xFF;
        U1IE = 0;
        U1IEbits.URSTIE = 1;
        U1IEbits.IDLEIE = 1;
        USBDeviceState = POWERED_STATE;
    }
    if(U1OTGIRbits.ACTVIF && U1OTGIEbits.ACTVIE)
    {
        U1OTGIEbits.ACTVIE = 0;
        USBClearInterruptFlag((uint8_t*)&U1OTGIR,4);
    }
    if(U1PWRCbits.USUSPEND==1)
    {
        return;
    }
    if(U1IRbits.URSTIF && U1IEbits.URSTIE)
    {
        USBDeviceInit();
        USBDeviceState = DEFAULT_STATE;
        BDT[EP0_OUT_EVEN].ADR = (uint8_t*)&SetupPkt;
        BDT[EP0_OUT_EVEN].CNT = 8;
        BDT[EP0_OUT_EVEN].STAT.Val &= ~_STAT_MASK;
        BDT[EP0_OUT_EVEN].STAT.Val |= _USIE|_DAT0|_DTSEN|_BSTALL;
    }
    if(U1IRbits.IDLEIF && U1IEbits.IDLEIE)
    {
        U1OTGIEbits.ACTVIE = 1;
        USBClearInterruptFlag((uint8_t*)&U1IR,4);
    }
    if(U1IRbits.SOFIF && U1IEbits.SOFIE)
    {
        USBClearInterruptFlag((uint8_t*)&U1IR,2);
    }
    if(U1IRbits.UERRIF && U1IEbits.UERRIE)
    {
        U1EIR = 0xFF;
    }
    if(U1IRbits.STALLIF && U1IEbits.STALLIE)
    {
        if(U1EP0bits.EPSTALL == 1)
        {
            if((pBDTEntryEP0OutCurrent->STAT.Val == _USIE) && (pBDTEntryIn[0]->STAT.Val == (_USIE|_BSTALL)))
            {
                pBDTEntryEP0OutCurrent->STAT.Val = _USIE|_DAT0|_DTSEN|_BSTALL;
            }//end if
            U1EP0bits.EPSTALL = 0;               // Clear stall status
        }//end if
        USBClearInterruptFlag((uint8_t*)&U1IR,7);
    }
    if(USBDeviceState < DEFAULT_STATE) return;
    if(U1IEbits.TRNIE)
    {
        for(i = 0; i < 4; i++)
        {
            if(U1IRbits.TRNIF)
            {
                USTATcopy = U1STAT;
                USBClearInterruptFlag((uint8_t*)&U1IR,3);
                USBCtrlEPService();
                USBEPService();
            }
            else
                break;
        }
    }
}


//void __attribute__((__interrupt__,__no_auto_psv__)) _USB1Interrupt(void)
//{
//    //printf("int\n");
//    USBDeviceTasks();
//    _USB1IF = 0;
//    _USB1IE = 1;
//}

void __attribute__((interrupt,auto_psv)) _AltUSB1Interrupt()
{
    //printf("int\n");
    USBDeviceTasks();
    _USB1IF = 0;
    _USB1IE = 1;
}