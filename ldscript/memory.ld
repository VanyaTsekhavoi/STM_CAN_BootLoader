/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = 0x20080000;    /* end of RAM */

/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x200;      /* required amount of heap  */
_Min_Stack_Size = 0x400; /* required amount of stack */

__app_start = ORIGIN(AFLASH);
__app_end = ORIGIN(AFLASH) + LENGTH(AFLASH);

__ldr_start = ORIGIN(LFLASH);
__ldr_end = ORIGIN(LFLASH) + LENGTH(LFLASH);

__cfg_ldr_start = ORIGIN(CLFLASH);
__cfg_ldr_end = ORIGIN(CLFLASH) + LENGTH(CLFLASH);

__cfg_app_start = ORIGIN(CAFLASH);
__cfg_app_end = ORIGIN(CAFLASH) + LENGTH(CAFLASH);

/* Specify the memory areas */
MEMORY
{
RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 512K
LFLASH (rx)       : ORIGIN = 0x08000000, LENGTH = 256K
CLFLASH (rx)      : ORIGIN = 0x08010000, LENGTH = 256K
CAFLASH (rx)      : ORIGIN = 0x08018000, LENGTH = 256K
AFLASH (rx)       : ORIGIN = 0x08040000, LENGTH = 1280K
}

