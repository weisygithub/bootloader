#include "cmdhandler.h"
#include "usb_hid.h"

struct script_table_t * ScriptTable;
uint8_t __attribute__ ((section ("buffer_space"))) _gblIntRamStorage[INTERNAL_SRAM_BUFFER_SIZE];
uint8_t *  uc_download_buffer; // Download Data Buffer
uint8_t *  uc_upload_buffer;       // Upload Data Buffer
uint8_t *  uc_script_buffer;       // Script Buffer
uint8_t *  uc_ScriptBuf_ptr;
uint8_t *  inbuffer;
uint8_t *  outbuffer;

void InitScripting(void)
{
    uc_script_buffer = _gblIntRamStorage;
    uc_ScriptBuf_ptr = uc_script_buffer;
    uc_download_buffer = uc_script_buffer + SCRIPTBUF_SIZE;
    uc_upload_buffer = uc_download_buffer + DOWNLOAD_SIZE;
    inbuffer = uc_upload_buffer + UPLOAD_SIZE;
    outbuffer = inbuffer + BUF_SIZE;
    ScriptTable = (struct script_table_t *) (outbuffer + BUF_SIZE);
}

void CmdHandler(void)
{
    uint8_t script = 0;
    uint16_t buffer[2];
    uint32_t i = 0;
    uint8_t length = 0;
    uint32_t flashaddress = 0;
    USBGet(inbuffer,BUF_SIZE);
    script = inbuffer[0];
    switch(script)
    {
        case READ_MEMOERY:
            for(i = 0;i < 0x200;i++)
            {
                ReadMemory(0x2400 + i);
            }
            printf("read memory\n");
            break;

        case PROGRAM_FLASH:
            length = inbuffer[1];
            flashaddress = ((uint32_t)inbuffer[2]) | ((uint32_t)inbuffer[3] << 8) | 
                    ((uint32_t)inbuffer[4] << 16) | ((uint32_t)inbuffer[5] << 24);
#ifdef DEBUG
            printf("%4x %4x ",(uint16_t)(flashaddress >> 16),(uint16_t)flashaddress);
#endif
            flashaddress >>= 1;
#ifdef DEBUG
            printf("%4x ",(uint16_t)flashaddress);
#endif
            for(i = 0;i < (length - 4) / 4;i++)
            {
                buffer[0] = (uint16_t)inbuffer[6 + 4 * i] | ((uint16_t)inbuffer[7 + 4 * i] << 8);
                buffer[1] = (uint16_t)inbuffer[8 + 4 * i] | ((uint16_t)inbuffer[9 + 4 * i] << 8);
                WriteMemory(buffer,flashaddress);
                flashaddress++;
                flashaddress++;
            }
            NVMCONbits.WREN = 0;
            printf("program memory\n");
            //asm("reset");
            break;
            
        case ERASE_MEMORY:
            for(i = 0;i < 0x20000 / 0x400;i++)
            {
                EraseMemory(0x2400 + i * 0x400);
                NVMCONbits.WREN = 0;
            }
            printf("erase memory\n");
            break;
            
        default:
            break;
    }
}

void ProcessCommand(void)
{
    while(USBDataReady())
    {
        CmdHandler();
    }
}

void ReadMemory(uint32_t address)
{
    uint16_t data[2];
    TBLPAG = ((uint16_t)(address >> 16));
    data[0] = __builtin_tblrdl(address);
    data[1] = __builtin_tblrdh(address);
#ifdef DEBUG
    printf("%4x, %4x, ",data[0],data[1]);
#endif
}
void WriteMemory(uint16_t *buffer,uint32_t address)
{
    TBLPAG = (uint16_t)(address >> 16);
    __builtin_tblwtl((uint16_t)address,buffer[0]);
    __builtin_tblwth((uint16_t)address,buffer[1]);
    NVMCON = 0x4003;
    asm("DISI #16");
    __builtin_write_NVM();
    //while(NVMCONbits.WR == 1);
}
void EraseMemory(uint32_t address)
{
    TBLPAG = (uint16_t)(address >> 16);
    __builtin_tblwtl((uint16_t)address, 0x0000);
    NVMCON = 0x4042;
    asm("DISI #5");
    __builtin_write_NVM();
    //while(NVMCONbits.WR == 1);
}