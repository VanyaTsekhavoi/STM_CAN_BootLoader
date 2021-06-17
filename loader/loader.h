#ifndef __LOADER__H__
#define __LOADER__H__

#include <stdint.h>
#include <stdbool.h>


uint32_t app_start(void);
uint32_t app_end(void);
uint32_t app_size(void);
uint32_t ldr_start(void);
uint32_t ldr_end(void);
uint32_t ldr_size(void);
void go_to_app(void);
void go_to_ldr(void);


#endif
