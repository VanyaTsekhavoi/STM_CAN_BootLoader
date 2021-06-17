#include "systemTimer.h"

inline void timebase_init()
{
    SysTick->LOAD = 0xffffffff;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

inline int32_t timebase_mark(int32_t *m)
{
    int32_t mark = (1 << 24) - SysTick->VAL - 1;
    int32_t prev = *m;
    *m = mark;

    return (mark - prev + (1 << 24)) & ((1 << 24) - 1);
}