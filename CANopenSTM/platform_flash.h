#ifndef __PLATFORM_FLASH_H__
#define __PLATFORM_FLASH_H__

#include "stm32f7xx_hal.h"
#include "stm32f7xx.h"
#include <stdint.h>
#include <stdbool.h>

void platform_flash_erase_flag_reset(void);
void platform_flash_unlock(void);
void platform_flash_lock(void);
void platform_deinit(void);
void platform_flash_erase_sector(int sector);
void platform_flash_erase_range(uint32_t dest, int sz);
void platform_flash_write(uint32_t dest, uint8_t *src, int sz);
int platform_flash_find_sector(uint32_t addr);

#endif
