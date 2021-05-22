#ifndef __LOADER__H__
#define __LOADER__H__

#include <stdint.h>
#include <stdbool.h>


uint32_t app_start();
uint32_t app_end();
uint32_t app_size();
uint32_t ldr_start();
uint32_t ldr_end();
uint32_t ldr_size();
void go_to_app();
void go_to_ldr();


#endif
