#ifndef CMD_HANDLER_H
#define	CMD_HANDLER_H

#include "system.h"

#define INTERNAL_SRAM_BUFFER_SIZE 0x800
#define SCRIPTBUF_SIZE 768
#define DOWNLOAD_SIZE  256
#define UPLOAD_SIZE 128
#define BUF_SIZE    64

struct script_table_t
{
    unsigned char Length;
    int StartIndex;
};

void InitScripting(void);
void ProcessCommand(void);
void ReadMemory(uint32_t address);
void WriteMemory(uint16_t *buffer,uint32_t address);
void EraseMemory(uint32_t address);

#define READ_MEMOERY 0x02
#define PROGRAM_FLASH 0x04
#define ERASE_MEMORY 0x03
#define JUMP 0x05

#define APP_ADDR 0x3000
#define PAGE_SIZE 0x400

#define DEBUG 1
#endif
