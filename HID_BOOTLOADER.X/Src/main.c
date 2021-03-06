// CONFIG3
#pragma config WPFP = WPFP511    //Write Protection Flash Page Segment Boundary->Highest Page (same as page 170)
#pragma config WPDIS = WPDIS    //Segment Write Protection Disable bit->Segmented code protection disabled
#pragma config WPCFG = WPCFGDIS    //Configuration Word Code Page Protection Select bit->Last page(at the top of program memory) and Flash configuration words are not protected
#pragma config WPEND = WPENDMEM    //Segment Write Protection End Page Select bit->Write Protect from WPFP to the last page of memory

// CONFIG2
#pragma config POSCMOD = NONE    //Primary Oscillator Select->Primary oscillator disabled
#pragma config DISUVREG = OFF    //Internal USB 3.3V Regulator Disable bit->Regulator is disabled
#pragma config IOL1WAY = OFF    //IOLOCK One-Way Set Enable bit->Write RP Registers Once
#pragma config OSCIOFNC = OFF    //Primary Oscillator Output Function->OSCO functions as CLKO (FOSC/2)
#pragma config FCKSM = CSDCMD    //Clock Switching and Monitor->Both Clock Switching and Fail-safe Clock Monitor are disabled
#pragma config FNOSC = FRCPLL    //Oscillator Select->Fast RC oscillator with Postscaler and PLL module (FRCPLL)
#pragma config PLL_96MHZ = ON    //96MHz PLL Disable->Enabled
#pragma config PLLDIV = NODIV    //USB 96 MHz PLL Prescaler Select bits->Oscillator input used directly (4MHz input)
#pragma config IESO = ON    //Internal External Switch Over Mode->IESO mode (Two-speed start-up) enabled

// CONFIG1
#pragma config WDTPS = PS32768    //Watchdog Timer Postscaler->1:32768
#pragma config FWPSA = PR128    //WDT Prescaler->Prescaler ratio of 1:128
#pragma config WINDIS = OFF    //Watchdog Timer Window->Standard Watchdog Timer enabled,(Windowed-mode is disabled)
#pragma config FWDTEN = OFF    //Watchdog Timer Enable->Watchdog Timer is disabled
#pragma config ICS = PGx1    //Comm Channel Select->Emulator functions are shared with PGEC1/PGED1
#pragma config BKBUG = OFF    //Background Debug->Device resets into Operational mode
#pragma config GWRP = OFF    //General Code Segment Write Protect->Writes to program memory are allowed
#pragma config GCP = OFF    //General Code Segment Code Protect->Code protection is disabled
#pragma config JTAGEN = OFF    //JTAG Port Enable->JTAG port is disabled

#include "usb_device.h"
#include "cmdhandler.h"
#include "uart.h"
extern uint8_t USBDeviceState;

int main(void)
{
    CLKDIV = 0x3100;
    INTCON2bits.ALTIVT = 1;
    UART1_Initialize();
    if((_RD2) && ((RCON & 0x83) != 0))  //button press
    {
        printf("jump\n");
        INTCON2bits.ALTIVT = 0;
        __asm__("goto 0x2600");
    }
    USBInitialize();
    InitScripting();
    while(1)
    {
        if(USBDeviceState < CONFIGURED_STATE)
        {
            USBDeviceTasks();
            if(USBDeviceState == CONFIGURED_STATE)
            {
                _USB1IE = 0;
                _USB1IF = 0;
                _USB1IP = 6;
                _USB1IE = 1;
            }
        }
        ProcessCommand();
    }
    return 1;
}

