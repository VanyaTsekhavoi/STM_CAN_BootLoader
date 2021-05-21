#include "stm32f7xx_hal.h"
#include "stm32f7xx.h"

#include "platform_flash.h"

typedef enum
{
	FL_HOMO,
	FL_HET
} flash_type_t;

typedef struct
{
	flash_type_t type;
	uint32_t start;
	uint32_t len;
	int num;
	int count;
	bool wr;
	bool erased;
} sector_t;

sector_t flash_desc[] = 
{
	{FL_HET, 0x08000000, 32 * 1024, FLASH_SECTOR_0, 1, false},
	{FL_HET, 0x08008000, 32 * 1024, FLASH_SECTOR_1, 1, false},
	{FL_HET, 0x08010000, 32 * 1024, FLASH_SECTOR_2, 1, true},
	{FL_HET, 0x08018000, 32 * 1024, FLASH_SECTOR_3, 1, true},
	{FL_HET, 0x08020000, 128 * 1024, FLASH_SECTOR_4, 1, true},
	{FL_HET, 0x08040000, 256 * 1024, FLASH_SECTOR_5, 1, true},
	{FL_HET, 0x08080000, 256 * 1024, FLASH_SECTOR_6, 1, true},
	{FL_HET, 0x080C0000, 256 * 1024, FLASH_SECTOR_7, 1, true},
	{FL_HET, 0x08100000, 256 * 1024, FLASH_SECTOR_8, 1, true},
	{FL_HET, 0x08140000, 256 * 1024, FLASH_SECTOR_9, 1, true},
	{FL_HET, 0x08180000, 256 * 1024, FLASH_SECTOR_10, 1, true},
	{FL_HET, 0x081C0000, 256 * 1024, FLASH_SECTOR_11, 1, true},
	{0, 0, 0, 1, false, false},
};

int platform_flash_find_sector(uint32_t addr)
{
	for(int i = 0; flash_desc[i].start; i++)
	{
		if((addr >= flash_desc[i].start) && (addr < (flash_desc[i].start + flash_desc[i].len)))
		{
			return i;
		}
	}
	return -1;
}


void platform_flash_erase_flag_reset()
{
	for(int i = 0; flash_desc[i].start; i++)
	{
		flash_desc[i].erased = false;
	}
}

void platform_flash_unlock()
{
	HAL_FLASH_Unlock();
}

void platform_flash_lock()
{
	HAL_FLASH_Lock();
}

void platform_flash_erase_sector(int sector)
{
	FLASH_Erase_Sector(sector, VOLTAGE_RANGE_3);
}

void platform_flash_erase_range(uint32_t dest, int sz)
{
	platform_flash_write(dest, NULL, sz);
}

void platform_flash_write(uint32_t dest, uint8_t *src, int sz)
{
	int s;

	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | 
			FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGAERR );


	for(int i = 0; i < sz; i++)
	{
		s = platform_flash_find_sector(dest + i);

		if(s < 0)
		{
			break;
		}
		if(!flash_desc[s].wr)
		{
			break;
		}

		if(!flash_desc[s].erased)
		{
			flash_desc[s].erased = true;
			platform_flash_erase_sector(flash_desc[s].num);
		}

		if(src)
		{
			HAL_FLASH_Program(TYPEPROGRAM_BYTE, dest + i, *(src + i));
		}
	}
}

void platform_deinit()
{
	__disable_irq();
	SCB_DisableICache();
	SCB_InvalidateICache();
	SysTick->CTRL = 0;
	HAL_DeInit();
	for (size_t i = 0; i < sizeof(NVIC->ICPR) / sizeof(NVIC->ICPR[0]); i++)
	{
		NVIC->ICPR[i] = 0xfffffffflu;
	}
	__enable_irq();
}
