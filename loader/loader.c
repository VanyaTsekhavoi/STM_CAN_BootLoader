#include "platform_flash.h"
#include "loader.h"

#include <stdint.h>

extern int __app_start;
extern int __app_end;
extern int __ldr_start;
extern int __ldr_end;


typedef void (*app_entry_t)(void);

uint32_t app_start()
{
	return (uint32_t)&__app_start;
}

uint32_t app_end()
{
	return (uint32_t)&__app_end;
}

uint32_t app_size()
{
	return ((uint32_t)&__app_end) - ((uint32_t)&__app_start);
}

uint32_t ldr_start()
{
	return (uint32_t)&__ldr_start;
}

uint32_t ldr_end()
{
	return (uint32_t)&__ldr_end;
}


uint32_t ldr_size()
{
	return ((uint32_t)&__ldr_end) - ((uint32_t)&__ldr_start);
}


void go_to_app()
{
	if(*(uint32_t *)app_start() != 0xffffffffu)
	{
		app_entry_t app = (app_entry_t)(*(uint32_t *)(app_start() + 4));
		platform_deinit();

		__set_MSP(*(uint32_t *)(app_start()));
		SCB->VTOR = app_start();
		app();
	}
}

void go_to_ldr()
{
	if(*(uint32_t *)ldr_start() != 0xffffffffu)
	{
		app_entry_t ldr = (app_entry_t)(*(uint32_t *)(ldr_start() + 4));
		platform_deinit();

		__set_MSP(*(uint32_t *)(ldr_start()));
		SCB->VTOR = ldr_start();
		ldr();
	}
}

