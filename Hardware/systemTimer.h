#ifndef SYSTIMER_H__
#define SYSTIMER_H__

#include "stm32f7xx_hal.h"
#include "stm32f7xx.h"

//1 - .c +. / static .h
//2 - inline linker error
//3 - statuc struct

//Initialise system timer
void timebase_init();

//Returns delta in ticks 
int32_t timebase_mark(int32_t *m);

typedef struct systemTimer
{
    uint32_t tick;
    uint32_t ticks_per_ms;
    uint32_t clk_mhz;
} sysTick_timer_param_t;

/** @} */
#endif
