#include "uart.h"

void UART1_Initialize(void)
{
    RPINR18bits.U1RXR = 0x0011;    //RF5->UART1:U1RX
    RPOR5bits.RP10R = 0x0003;    //RF4->UART1:U1TX    
    U1MODE = (0x8008 & ~(1<<15));
    U1STA = 0x00;
    U1BRG = 0x22;
    U1MODEbits.UARTEN = 1; 
    U1STAbits.UTXEN = 1;
}

uint8_t UART1_Read(void)
{
    while(!(U1STAbits.URXDA == 1))
    {
        
    }
    if ((U1STAbits.OERR == 1))
    {
        U1STAbits.OERR = 0;
    }
    return U1RXREG;
}

void UART1_Write(uint8_t txData)
{
    while(U1STAbits.UTXBF == 1)
    {
        
    }
    U1TXREG = txData;
}