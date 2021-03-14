#ifndef __USB_DESCRIPTORS_C
#define __USB_DESCRIPTORS_C

#include "usb_device.h"

/* Device Descriptor */
const USB_DEVICE_DESCRIPTOR device_dsc=
{
    0x12,                   // Size of this descriptor in bytes
    0x01,  // DEVICE descriptor type
    0x0200,                 // USB Spec Release Number in BCD format
    0x00,                   // Class Code
    0x00,                   // Subclass code
    0x00,                   // Protocol code
    0x08,                     // Max packet size for EP0, see usb_config.h
    0x2020,                     // Vendor ID
    0x0418,                    // Product ID
    0x0002,                 // Device release number in BCD format
    0x01,                   // Manufacturer string index
    0x02,                   // Product string index
    0x03,                   // Device serial number string index        
    0x01                    // Number of possible configurations
};

/* Configuration 1 Descriptor */
const uint8_t configDescriptor1[]={
    /* Configuration Descriptor */
    0x09,//sizeof(USB_CFG_DSC),    // Size of this descriptor in bytes
    0x02,                // CONFIGURATION descriptor type
    0x29,0x00,            // Total length of data for this cfg
    1,                      // Number of interfaces in this cfg
    1,                      // Index value of this configuration
    0,                      // Configuration string index
    0x80,               // Attributes, see usb_device.h
    250,                     // Max power consumption (2X mA)
                            
    /* Interface Descriptor */
    0x09,//sizeof(USB_INTF_DSC),   // Size of this descriptor in bytes
    0x04,               // INTERFACE descriptor type
    0,                      // Interface Number
    0,                      // Alternate Setting Number
    2,                      // Number of endpoints in this intf
    0x03,               // Class code
    0,     // Subclass code
    0,     // Protocol code
    0,                      // Interface string index

    /* HID Class-Specific Descriptor */
    0x09,//sizeof(USB_HID_DSC)+3,    // Size of this descriptor in bytes RRoj hack
    0x21,                // HID descriptor type
    0x11,0x01,                 // HID Spec Release Number in BCD format (1.11)
    0x00,                   // Country Code (0x00 for Not supported)
    1,         // Number of class descriptors, see usbcfg.h
    0x22,                // Report descriptor type
    0x1D,0x00,//sizeof(hid_rpt01),      // Size of the report descriptor
    
    /* Endpoint Descriptor */
    0x07,/*sizeof(USB_EP_DSC)*/
    0x05,    //Endpoint Descriptor
    0x81,                   //EndpointAddress
    0x03,                       //Attributes
    0x40,0x00,    //size
    0xFF,                        //Interval

    /* Endpoint Descriptor */
    0x07,/*sizeof(USB_EP_DSC)*/
    0x05,    //Endpoint Descriptor
    0x01,                   //EndpointAddress
    0x03,                       //Attributes
    0x40,0x00,   //size
    0x01                        //Interval
};

//Language code string descriptor
const struct{uint8_t bLength;uint8_t bDscType;uint16_t string[1];}sd000={
sizeof(sd000),
0x03,
{0x0409}
};

//Manufacturer string descriptor
const struct{uint8_t bLength;uint8_t bDscType;uint16_t string[25];}sd001={
sizeof(sd001),
0x03,
{'M','i','c','r','o','c','h','i','p',' ','T','e','c','h','n','o','l','o','g','y',' ','I','n','c','.'}
};

//Product string descriptor
const struct{uint8_t bLength;uint8_t bDscType;uint16_t string[9];}sd002={
sizeof(sd002),
0x03,
{'A','L','L','L','I','N','K','B','L'}
};

//Class specific descriptor - HID 
const struct{uint8_t report[29];}hid_rpt01={
{
    0x06, 0x00, 0xFF,       // Usage Page = 0xFFFF (Vendor Defined)
    0x09, 0x01,             // Usage 
    0xA1, 0x01,             // Collection (Application, probably not important because vendor defined usage)
    0x19, 0x01,             //      Usage Minimum (Vendor Usage = 0) (minimum bytes the device should send is 0)
    0x29, 0x40,             //      Usage Maximum (Vendor Usage = 64) (maximum bytes the device should send is 64)
    0x15, 0x00,             //      Logical Minimum (Vendor Usage = 0)
    0x26, 0xFF, 0x00,       //      Logical Maximum (Vendor Usage = 255)
    0x75, 0x08,             //      Report Size 8 bits (one full byte) for each report.
    0x95, 0x40,             //      Report Count 64 bytes in a full report.
    0x81, 0x02,             //      Input (Data, Var, Abs)
    0x19, 0x01,             //      Usage Minimum (Vendor Usage = 0)
    0x29, 0x40,             //      Usage Maximum (Vendor Usage = 64)
    0x91, 0x02,             //      Output (Data, Var, Ads)
    0xC0}
};                  // End Collection

//Array of configuration descriptors
const uint8_t *const USB_CD_Ptr[]=
{
    (const uint8_t *const)&configDescriptor1
};

//Array of string descriptors
const uint8_t *const USB_SD_Ptr[]=
{
    (const uint8_t *const)&sd000,  // Language code string descriptor
    (const uint8_t *const)&sd001,  // Manufacturer string descriptor
    (const uint8_t *const)&sd002,  // Product string descriptor
};
#endif
