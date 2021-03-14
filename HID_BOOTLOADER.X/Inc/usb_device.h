#ifndef USBDEVICE_H
#define USBDEVICE_H

#include "system.h"

typedef struct _USB_RX_INFO
{
    uint8_t waitingForFirstReport;
    uint32_t total;
    uint32_t readByUser;
    uint8_t available;
    uint8_t offsetIntoCopy;
    uint8_t rxPending;
    uint8_t reportCopyAvailable;
    uint8_t __attribute__ ((aligned (2))) reportCopy[64];
} USB_RX_INFO;

typedef struct _USB_TX_INFO
{
    uint16_t total;                 // how much data to send
    uint16_t offset;                // where to put the next char when USBPutCont() is called
    volatile bool SENDSSS;       
    volatile bool BufferRdy;     
    volatile bool FastPutInProgress;    // Possible to get back to back interrupts
    volatile uint8_t SSSPktsSent;           
    uint8_t __attribute__ ((aligned (4))) buffer[0x600 + 4 + 4 + 2];
} USB_TX_INFO;

typedef union _CTRL_TRF_SETUP
{
    struct
    {
        uint8_t bmRequestType; //from table 9-2 of USB2.0 spec
        uint8_t bRequest; //from table 9-2 of USB2.0 spec
        uint16_t wValue; //from table 9-2 of USB2.0 spec
        uint16_t wIndex; //from table 9-2 of USB2.0 spec
        uint16_t wLength; //from table 9-2 of USB2.0 spec
    };
    struct
    {
        unsigned :8;
        unsigned :8;
        WORD_VAL W_Value; //from table 9-2 of USB2.0 spec, allows byte/bitwise access
        WORD_VAL W_Index; //from table 9-2 of USB2.0 spec, allows byte/bitwise access
        WORD_VAL W_Length; //from table 9-2 of USB2.0 spec, allows byte/bitwise access
    };
    struct
    {
        unsigned Recipient:5;   //Device,Interface,Endpoint,Other
        unsigned RequestType:2; //Standard,Class,Vendor,Reserved
        unsigned DataDir:1;     //Host-to-device,Device-to-host
        unsigned :8;
        uint8_t bFeature;          //DEVICE_REMOTE_WAKEUP,ENDPOINT_HALT
        unsigned :8;
        unsigned :8;
        unsigned :8;
        unsigned :8;
        unsigned :8;
    };
    struct
    {
        unsigned :8;
        unsigned :8;
        uint8_t bDscIndex;         //For Configuration and String DSC Only
        uint8_t bDescriptorType;          //Device,Configuration,String
        uint16_t wLangID;           //Language ID
        unsigned :8;
        unsigned :8;
    };
    struct
    {
        unsigned :8;
        unsigned :8;
        BYTE_VAL bDevADR;       //Device Address 0-127
        uint8_t bDevADRH;          //Must equal zero
        unsigned :8;
        unsigned :8;
        unsigned :8;
        unsigned :8;
    };
    struct
    {
        unsigned :8;
        unsigned :8;
        uint8_t bConfigurationValue;         //Configuration Value 0-255
        uint8_t bCfgRSD;           //Must equal zero (Reserved)
        unsigned :8;
        unsigned :8;
        unsigned :8;
        unsigned :8;
    };
    struct
    {
        unsigned :8;
        unsigned :8;
        uint8_t bAltID;            //Alternate Setting Value 0-255
        uint8_t bAltID_H;          //Must equal zero
        uint8_t bIntfID;           //Interface Number Value 0-255
        uint8_t bIntfID_H;         //Must equal zero
        unsigned :8;
        unsigned :8;
    };
    struct
    {
        unsigned :8;
        unsigned :8;
        unsigned :8;
        unsigned :8;
        uint8_t bEPID;             //Endpoint ID (Number & Direction)
        uint8_t bEPID_H;           //Must equal zero
        unsigned :8;
        unsigned :8;
    };
    struct
    {
        unsigned :8;
        unsigned :8;
        unsigned :8;
        unsigned :8;
        unsigned EPNum:4;       //Endpoint Number 0-15
        unsigned :3;
        unsigned EPDir:1;       //Endpoint Direction: 0-OUT, 1-IN
        unsigned :8;
        unsigned :8;
        unsigned :8;
    };
} CTRL_TRF_SETUP;

typedef union _BD_STAT
{
    struct{
        unsigned            :2;      //Byte count
        unsigned    BSTALL  :1;     //Buffer Stall Enable
        unsigned    DTSEN   :1;     //Data Toggle Synch Enable
        unsigned            :2;     //Reserved - write as 00
        unsigned    DTS     :1;     //Data Toggle Synch Value
        unsigned    UOWN    :1;     //USB Ownership
    };
    struct{
        unsigned            :2;
        unsigned    PID0    :1;
        unsigned    PID1    :1;
        unsigned    PID2    :1;
        unsigned    PID3    :1;
    };
    struct{
        unsigned            :2;
        unsigned    PID     :4;     // Packet Identifier
    };
    uint8_t            Val;
} BD_STAT;                      //Buffer Descriptor Status Register

typedef union __BDT
{
    union
    {
        struct
        {
            uint8_t CNT;//         __attribute__ ((packed));
            BD_STAT     STAT __attribute__ ((packed));
        };
        struct
        {
            uint16_t        count:10;   //test
            uint8_t        :6;
            uint8_t*       ADR; //Buffer Address
        };
    };
    uint32_t           Val;
    uint16_t           v[2];
} BDT_ENTRY;

typedef struct
{
    union
    {
        uint8_t *bRam;
        const uint8_t *bRom;
        uint16_t *wRam;
        const uint16_t *wRom;
    }pSrc;
    union
    {
        struct
        {
            uint8_t ctrl_trf_mem          :1;
            uint8_t reserved              :5;
            uint8_t includeZero           :1;
            uint8_t busy                  :1;
        }bits;
        uint8_t Val;
    }info;
    WORD_VAL wCount;
}IN_PIPE;

typedef struct
{
    union
    {
        uint8_t *bRam;
        uint16_t *wRam;
    }pDst;
    union
    {
        struct
        {
            uint8_t reserved              :7;
            uint8_t busy                  :1;
        }bits;
        uint8_t Val;
    }info;
    WORD_VAL wCount;
    void (*pFunc)(void);
}OUT_PIPE;

typedef struct __attribute__ ((packed)) _USB_DEVICE_DESCRIPTOR
{
    uint8_t bLength;               // Length of this descriptor.
    uint8_t bDescriptorType;       // DEVICE descriptor type (USB_DESCRIPTOR_DEVICE).
    uint16_t bcdUSB;                // USB Spec Release Number (BCD).
    uint8_t bDeviceClass;          // Class code (assigned by the USB-IF). 0xFF-Vendor specific.
    uint8_t bDeviceSubClass;       // Subclass code (assigned by the USB-IF).
    uint8_t bDeviceProtocol;       // Protocol code (assigned by the USB-IF). 0xFF-Vendor specific.
    uint8_t bMaxPacketSize0;       // Maximum packet size for endpoint 0.
    uint16_t idVendor;              // Vendor ID (assigned by the USB-IF).
    uint16_t idProduct;             // Product ID (assigned by the manufacturer).
    uint16_t bcdDevice;             // Device release number (BCD).
    uint8_t iManufacturer;         // Index of String Descriptor describing the manufacturer.
    uint8_t iProduct;              // Index of String Descriptor describing the product.
    uint8_t iSerialNumber;         // Index of String Descriptor with the device's serial number.
    uint8_t bNumConfigurations;    // Number of possible configurations.
} USB_DEVICE_DESCRIPTOR;

#define _BSTALL     0x04        //Buffer Stall enable
#define _DTSEN      0x08        //Data Toggle Synch enable
#define _DAT0       0x00        //DATA0 packet expected next
#define _DAT1       0x40        //DATA1 packet expected next
#define _DTSMASK    0x40        //DTS Mask
#define _USIE       0x80        //SIE owns buffer
#define _UCPU       0x00        //CPU owns buffer
#define _STAT_MASK  0xFC

#define USTAT_EP0_PP_MASK   ~0x04
#define USTAT_EP_MASK       0xFC
#define USTAT_EP0_OUT_EVEN  0x00
#define USTAT_EP0_IN        0x08
#define USTAT_EP1_PP_MASK   ~0x04  
#define USTAT_EP1_OUT_EVEN  0x10
#define USTAT_EP1_IN        0x18
#define UEP_STALL 0x0002

#define USB_INPIPES_ROM            0x00     //Data comes from RAM
#define USB_INPIPES_RAM            0x01     //Data comes from ROM
#define USB_INPIPES_BUSY           0x80     //The PIPE is busy
#define USB_INPIPES_INCLUDE_ZERO   0x40     //include a trailing zero packet
#define USB_INPIPES_NO_DATA        0x00     //no data to send
#define USB_INPIPES_NO_OPTIONS     0x00     //no options set

#define GET_STATUS  0
#define CLR_FEATURE 1
#define SET_FEATURE 3
#define SET_ADR     5
#define GET_DSC     6
#define SET_DSC     7
#define GET_CFG     8
#define SET_CFG     9
#define GET_INTF    10
#define SET_INTF    11
#define SYNCH_FRAME 12

#define DEVICE_REMOTE_WAKEUP    0x01
#define ENDPOINT_HALT           0x00

#define DETACHED_STATE          0x00
#define ATTACHED_STATE          0x01
#define POWERED_STATE           0x02
#define DEFAULT_STATE           0x04
#define ADR_PENDING_STATE       0x08
#define ADDRESS_STATE           0x10
#define CONFIGURED_STATE        0x20

#define EP_CTRL     0x0C            // Cfg Control pipe for this ep
#define EP_OUT      0x18            // Cfg OUT only pipe for this ep
#define EP_IN       0x14            // Cfg IN only pipe for this ep
#define EP_OUT_IN   0x1C            // Cfg both OUT & IN pipes for this ep
#define HSHK_EN     0x01            // Enable handshake packet

#define USB_HANDSHAKE_ENABLED   0x01
#define USB_OUT_ENABLED         0x08
#define USB_IN_ENABLED          0x04
#define USB_ALLOW_SETUP         0x00
#define USB_DISALLOW_SETUP      0x10

#define WAIT_SETUP          0
#define CTRL_TRF_TX         1
#define CTRL_TRF_RX         2

#define SHORT_PKT_NOT_USED  0
#define SHORT_PKT_PENDING   1
#define SHORT_PKT_SENT      2

#define SETUP_TOKEN         0x0D    // 0b00001101
#define OUT_TOKEN           0x01    // 0b00000001
#define IN_TOKEN            0x09    // 0b00001001

#define HOST_TO_DEV         0
#define DEV_TO_HOST         1

#define STANDARD            0x00
#define CLASS               0x01
#define VENDOR              0x02

#define RCPT_DEV            0
#define RCPT_INTF           1
#define RCPT_EP             2
#define RCPT_OTH            3

#define USBInitEP(a)

#define USB_NEXT_EP0_OUT_PING_PONG 0x0004
#define USB_NEXT_EP0_IN_PING_PONG 0x0004
#define USB_NEXT_PING_PONG 0x0004
#define EP0_OUT_EVEN    0
#define EP0_OUT_ODD     1
#define EP0_IN_EVEN     2
#define EP0_IN_ODD      3
#define EP1_OUT_EVEN    4
#define EP1_OUT_ODD     5
#define EP1_IN_EVEN     6
#define EP1_IN_ODD      7

void USBInitTxVars(void);
void USBDeviceTasks(void);
void USBInitialize(void);
volatile BDT_ENTRY* USBTransferOnePacket(uint8_t ep,uint8_t dir,uint8_t* data,uint8_t len);
void USBHandleOneRxReport(void);
#endif
