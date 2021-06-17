#ifndef CANOPEN_LINK_H__
#define CANOPEN_LINK_H__

#include "CANopen.h"
#include "can.h"

void canopen_link_init(void);
void canopen_link_poll(void);
CO_ReturnError_t CO_init(
    int32_t CANbaseAddress,
    uint8_t nodeId,
    uint16_t bitRate);

#endif
