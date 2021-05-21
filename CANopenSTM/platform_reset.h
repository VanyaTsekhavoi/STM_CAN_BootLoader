#ifndef PLATFORM_DO_RESET_H__
#define PLATFORM_DO_RESET_H__

#include "stm32f7xx_hal.h"

static inline void platform_do_reset()
{
	SCB->AIRCR = 0x5fa << SCB_AIRCR_VECTKEY_Pos | SCB_AIRCR_SYSRESETREQ_Msk;
}


#endif
