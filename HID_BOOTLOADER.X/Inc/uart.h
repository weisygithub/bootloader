#ifndef UART_H
#define UART_H

#include "system.h"

void UART1_Initialize(void);
uint8_t UART1_Read(void);
void UART1_Write(uint8_t txData);

#endif