/*
 * CAN module object for generic microcontroller.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver.c
 * @ingroup     CO_driver
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "301/CO_driver.h"
#include "301/CO_Emergency.h"
#include "can.h"

static void delay(uint32_t d)
{
    for (; d--;)
    {
        asm(" nop\r\n");
    }
}

static inline bool can_enter_init_mode(CAN_TypeDef *dev)
{
    dev->MCR |= CAN_MCR_INRQ;
    for (int i = 0; i < 10; i++)
    {
        delay(10000);
        if (dev->MSR & CAN_MSR_INAK)
        {
            return true;
        }
    }
    return false;
}

static inline void can_leave_init_mode(CAN_TypeDef *dev)
{
    uint32_t reg;
    reg = dev->BTR;
    reg &= ~CAN_BTR_SILM_Msk;
    reg &= ~CAN_BTR_LBKM_Msk;
    dev->BTR = reg;

    dev->MCR &= ~CAN_MCR_INRQ;
}

static int32_t can_set_bitrate(CAN_TypeDef *dev, int32_t bit_rate)
{
    int32_t res = 0;

    if (can_enter_init_mode(dev))
    {
        do
        {
            int32_t tqs;
            int32_t brp;
            int32_t tseg1, tseg2;

#ifdef STM32F767xx
            int32_t FOSC = HAL_RCC_GetPCLK1Freq();
#else
#error Unknown type of Processor. Please, check CAN input clock frequency source(xxxCLK)!
#endif

            uint32_t reg;

            for (brp = 1; brp < 0x400; brp++)
            {
                tqs = FOSC / brp / bit_rate;
                if (tqs >= 5 && tqs <= 16 && (FOSC / brp / tqs == bit_rate))
                {
                    break;
                }
            }
            if (brp >= 0x400)
            {
                break;
            }

            tseg1 = tseg2 = (tqs - 1) / 2;
            if (tqs - (1 + tseg1 + tseg2) == 1)
            {
                tseg1++;
            }

            if (tseg2 > 8)
            {
                tseg1 += tseg2 - 8;
                tseg2 = 8;
            }

            reg = dev->BTR;
            reg &= ~(0x3ff | 0x7f << 16);
            reg |= (brp - 1) | (tseg1 - 1) << 16 | (tseg2 - 1) << 20;
            dev->BTR = reg;

            res = bit_rate;
        } while (false);
    }

    return res;
}

static void can_start(CAN_TypeDef *dev)
{
    dev->IER |= CAN_IER_LECIE | CAN_IER_BOFIE | CAN_IER_EPVIE | CAN_IER_EWGIE;

    dev->MCR &= ~CAN_MCR_NART;  //disable no autoretransmitt
    dev->MCR |= CAN_MCR_ABOM;   //autoble bus off management
    dev->MCR &= ~CAN_MCR_SLEEP; //disable sleep mode

    SET_BIT(dev->FMR, CAN_FMR_FINIT);

    //deactivate all filters
    dev->FA1R = 0;
    //assign all filters to master CAN (CAN1, CAN3, etc.)
    CLEAR_BIT(dev->FMR, CAN_FMR_CAN2SB);
    SET_BIT(dev->FMR, 28 << CAN_FMR_CAN2SB_Pos);
    //all filters in mask mode
    dev->FM1R = 0;
    //all filters in single 32bit mode
    dev->FS1R = 0x0fffffffu;
    //assign all filters to FIFO0
    dev->FFA1R = 0x0u;

    //clear all ids & masks
    for (int i = 0; i < 28; i++)
    {
        dev->sFilterRegister[i].FR1 = 0;
        dev->sFilterRegister[i].FR2 = 0;
    }

    CLEAR_BIT(dev->FMR, CAN_FMR_FINIT);
}

static void can_reset_module(CAN_TypeDef *dev)
{
    SET_BIT(dev->MCR, CAN_MCR_RESET);
    while (dev->MCR & CAN_MCR_RESET)
        ;
}

static void can_set_rx_filter(CAN_TypeDef *dev, int filter, uint32_t id, uint32_t mask)
{
    SET_BIT(dev->FMR, CAN_FMR_FINIT);
    CLEAR_BIT(dev->FA1R, 1 << filter);
    dev->sFilterRegister[filter].FR1 = id;
    dev->sFilterRegister[filter].FR2 = mask;
    CLEAR_BIT(dev->FM1R, 1 << filter);
    CLEAR_BIT(dev->FFA1R, 1 << filter);
    SET_BIT(dev->FA1R, 1 << filter);
    CLEAR_BIT(dev->FMR, CAN_FMR_FINIT);
}

static inline bool can_tx(CAN_TypeDef *dev, uint32_t ident, int dlc, uint8_t *d)
{
    uint32_t txmb = -1;

    if ((dev->TSR & CAN_TSR_TME0) == CAN_TSR_TME0)
    {
        txmb = 0U;
    }
    else if ((dev->TSR & CAN_TSR_TME1) == CAN_TSR_TME1)
    {
        txmb = 1U;
    }
    else if ((dev->TSR & CAN_TSR_TME2) == CAN_TSR_TME2)
    {
        txmb = 2U;
    }
    else
    {
        return false;
    }

    dev->sTxMailBox[txmb].TIR &= CAN_TI0R_TXRQ;
    dev->sTxMailBox[txmb].TIR = ident;

    dev->sTxMailBox[txmb].TDTR &= 0xFFFFFFF0U;
    dev->sTxMailBox[txmb].TDTR |= dlc;

    dev->sTxMailBox[txmb].TDLR =
        (((uint32_t)d[3] << 24U) |
         ((uint32_t)d[2] << 16U) |
         ((uint32_t)d[1] << 8U) |
         ((uint32_t)d[0]));
    dev->sTxMailBox[txmb].TDHR =
        (((uint32_t)d[7] << 24U) |
         ((uint32_t)d[6] << 16U) |
         ((uint32_t)d[5] << 8U) |
         ((uint32_t)d[4]));

    dev->sTxMailBox[txmb].TIR |= CAN_TI0R_TXRQ;

    return true;
}

static inline bool can_is_transmitting(CAN_TypeDef *dev)
{
    return (dev->TSR & (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2)) !=
           (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2);
}

static inline void can_tx_abort(CAN_TypeDef *dev)
{
    dev->TSR |= CAN_TSR_ABRQ0 | CAN_TSR_ABRQ1 | CAN_TSR_ABRQ2;
    while (dev->TSR & (CAN_TSR_ABRQ0 | CAN_TSR_ABRQ1 | CAN_TSR_ABRQ2))
        ;
}

static inline bool can_is_rx_overrun(CAN_TypeDef *dev)
{
    if (dev->RF0R & (CAN_RF0R_FOVR0))
    {
        dev->RF0R |= CAN_RF0R_FOVR0;
        return true;
    }

    return false;
}

static inline bool can_is_err_warning_level(CAN_TypeDef *dev)
{
    if (dev->ESR & CAN_ESR_EWGF)
    {
        dev->ESR &= ~CAN_ESR_EWGF;
        return true;
    }
    return false;
}

static inline bool can_is_err_passive_level(CAN_TypeDef *dev)
{
    if (dev->ESR & CAN_ESR_EPVF)
    {
        dev->ESR &= ~CAN_ESR_EPVF;
        return true;
    }
    return false;
}

static inline bool can_is_bus_off(CAN_TypeDef *dev)
{
    if (dev->ESR & CAN_ESR_BOFF)
    {
        dev->ESR &= ~CAN_ESR_BOFF;
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline uint16_t can_rx_error_counter(CAN_TypeDef *dev)
{
    return (dev->ESR & CAN_ESR_REC) >> CAN_ESR_REC_Pos;
}

static inline uint16_t can_tx_error_counter(CAN_TypeDef *dev)
{
    return (dev->ESR & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static inline bool can_is_error_occured(CAN_TypeDef *dev)
{
    return (dev->MSR & CAN_MSR_ERRI);
}

static inline void can_clear_error_status(CAN_TypeDef *dev)
{
    dev->MSR |= CAN_MSR_ERRI;
}

static inline bool can_is_rx_pending(CAN_TypeDef *dev)
{
    return ((dev->RF0R & CAN_RF0R_FMP0_Msk) > 0);
}

static inline int can_nbr_of_rx_pending(CAN_TypeDef *dev)
{
    return (dev->RF0R & CAN_RF0R_FMP0_Msk);
}

static inline int can_rx_get_dlc(CAN_TypeDef *dev)
{
    return dev->sFIFOMailBox[0].RDTR & 0xff;
}

static inline int can_rx_get_filter_index(CAN_TypeDef *dev)
{
    return (dev->sFIFOMailBox[0].RDTR >> 8) & 0xff;
}

static inline uint32_t can_rx_get_ident(CAN_TypeDef *dev)
{
    return dev->sFIFOMailBox[0].RIR;
}

static inline void can_rx_get_data(CAN_TypeDef *dev, uint8_t *data)
{
    data[0] = (uint8_t)0xFFU & dev->sFIFOMailBox[0].RDLR;
    data[1] = (uint8_t)0xFFU & (dev->sFIFOMailBox[0].RDLR >> 8U);
    data[2] = (uint8_t)0xFFU & (dev->sFIFOMailBox[0].RDLR >> 16U);
    data[3] = (uint8_t)0xFFU & (dev->sFIFOMailBox[0].RDLR >> 24U);
    data[4] = (uint8_t)0xFFU & dev->sFIFOMailBox[0].RDHR;
    data[5] = (uint8_t)0xFFU & (dev->sFIFOMailBox[0].RDHR >> 8U);
    data[6] = (uint8_t)0xFFU & (dev->sFIFOMailBox[0].RDHR >> 16U);
    data[7] = (uint8_t)0xFFU & (dev->sFIFOMailBox[0].RDHR >> 24U);
}

static inline void can_release_rx_message(CAN_TypeDef *dev)
{
    dev->RF0R = CAN_RF0R_RFOM0;
}

static inline uint32_t can_is_message_sent(CAN_TypeDef *dev)
{
    if (dev->TSR & (CAN_TSR_RQCP0))
    {
        return dev->TSR & CAN_TSR_RQCP0;
    }

    if (dev->TSR & (CAN_TSR_RQCP1))
    {
        return dev->TSR & CAN_TSR_RQCP1;
    }

    if (dev->TSR & (CAN_TSR_RQCP2))
    {
        return dev->TSR & CAN_TSR_RQCP2;
    }
    return 0;
}

static inline void can_release_tx_message(CAN_TypeDef *dev, uint32_t mask)
{
    dev->TSR |= mask;
}

/******************************************************************************/
void CO_CANsetConfigurationMode(void *CANptr)
{
    /* Put CAN module in configuration mode */
    can_enter_init_mode((CAN_TypeDef *)CANptr);
}

/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule)
{
    /* Put CAN module in normal mode */

    can_leave_init_mode((CAN_TypeDef *)CANmodule->CANptr);
    CANmodule->CANnormal = true;
}

/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(
    CO_CANmodule_t *CANmodule,
    void *CANptr,
    CO_CANrx_t rxArray[],
    uint16_t rxSize,
    CO_CANtx_t txArray[],
    uint16_t txSize,
    uint16_t CANbitRate)
{
    uint16_t i;

    /* verify arguments */
    if (CANmodule == NULL || rxArray == NULL || txArray == NULL)
    {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    CANmodule->CANptr = CANptr;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANerrorStatus = 0;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = (rxSize <= 28U) ? true : false; /* microcontroller dependent */
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;

    for (i = 0U; i < rxSize; i++)
    {
        rxArray[i].ident = 0U;
        rxArray[i].mask = 0U;
        rxArray[i].object = NULL;
        rxArray[i].CANrx_callback = NULL;
    }
    for (i = 0U; i < txSize; i++)
    {
        txArray[i].bufferFull = false;
    }

    /* Configure CAN module registers */

    can_start((CAN_TypeDef *)CANptr);

    /* Configure CAN timing */

    if (can_set_bitrate((CAN_TypeDef *)CANptr, CANbitRate * 1000) != CANbitRate * 1000)
    {
        return CO_ERROR_ILLEGAL_BAUDRATE;
    }

    /* Configure CAN module hardware filters */
    if (CANmodule->useCANrxFilters)
    {
        /* CAN module filters are used, they will be configured with */
        /* CO_CANrxBufferInit() functions, called by separate CANopen */
        /* init functions. */
        /* Configure all masks so, that received message must match filter */
    }
    else
    {
        /* CAN module filters are not used, all messages with standard 11-bit */
        /* identifier will be received */
        /* Configure mask 0 so, that all messages with standard identifier are accepted */

        /* set filter0 as allpass*/
        can_set_rx_filter((CAN_TypeDef *)CANptr, 0, 0, 0);
    }

    /* configure CAN interrupt registers */

    return CO_ERROR_NO;
}

/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
    /* turn off the module */
    can_reset_module((CAN_TypeDef *)CANmodule->CANptr);
}

/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(
    CO_CANmodule_t *CANmodule,
    uint16_t index,
    uint16_t ident,
    uint16_t mask,
    bool_t rtr,
    void *object,
    void (*CANrx_callback)(void *object, void *message))
{
    CO_ReturnError_t ret = CO_ERROR_NO;
    CAN_TypeDef *dev = (CAN_TypeDef *)CANmodule->CANptr;

    if ((CANmodule != NULL) && (object != NULL) && (CANrx_callback != NULL) && (index < CANmodule->rxSize))
    {
        /* buffer, which will be configured */
        CO_CANrx_t *buffer = &CANmodule->rxArray[index];

        /* Configure object variables */
        buffer->object = object;
        buffer->CANrx_callback = CANrx_callback;

        /* CAN identifier and CAN mask, bit aligned with CAN module. Different on different microcontrollers. */
        buffer->ident = ((uint32_t)(ident & 0x07FFU)) << CAN_RI0R_STID_Pos;
        if (rtr)
        {
            buffer->ident |= CAN_RI0R_RTR_Msk;
        }
        //IDE (0x4) & RTR (0x2) have to match
        buffer->mask = ((uint32_t)(mask & 0x07FFU)) << CAN_RI0R_STID_Pos |
                       CAN_RI0R_RTR_Msk | CAN_RI0R_IDE_Msk;

        /* Set CAN hardware module filter and mask. */
        if (CANmodule->useCANrxFilters)
        {
            can_set_rx_filter(dev, index, buffer->ident, buffer->mask);
        }
    }
    else
    {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}

/******************************************************************************/
CO_CANtx_t *CO_CANtxBufferInit(
    CO_CANmodule_t *CANmodule,
    uint16_t index,
    uint16_t ident,
    bool_t rtr,
    uint8_t noOfBytes,
    bool_t syncFlag)
{
    CO_CANtx_t *buffer = NULL;

    if ((CANmodule != NULL) && (index < CANmodule->txSize))
    {
        /* get specific buffer */
        buffer = &CANmodule->txArray[index];

        /* CAN identifier, DLC and rtr, bit aligned with CAN module transmit buffer.
         * Microcontroller specific. */
        buffer->ident = ((uint32_t)(ident & 0x07FFU)) << CAN_TI0R_STID_Pos |
                        (rtr ? CAN_TI0R_RTR_Msk : 0x0);
        buffer->DLC = noOfBytes & 0xf;

        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}

/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CO_ReturnError_t err = CO_ERROR_NO;
    CAN_TypeDef *dev = (CAN_TypeDef *)CANmodule->CANptr;

    /* Verify overflow */
    if (buffer->bufferFull)
    {
        if (!CANmodule->firstCANtxMessage)
        {
            /* don't set error, if bootup message is still on buffers */
            CANmodule->CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
        }
        err = CO_ERROR_TX_OVERFLOW;
    }

    CO_LOCK_CAN_SEND();
    /* if CAN TX buffer is free, copy message to it */
    if (can_tx(dev, buffer->ident, buffer->DLC, buffer->data) &&
    CANmodule->CANtxCount == 0)
    {
        CANmodule->bufferInhibitFlag = buffer->syncFlag;
        /* copy message and txRequest */
    }
    /* if no buffer is free, message will be sent by interrupt */
    else
    {
        buffer->bufferFull = true;
        CANmodule->CANtxCount++;
    }
    CO_UNLOCK_CAN_SEND();

    return err;
}

/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
    uint32_t tpdoDeleted = 0U;
    CAN_TypeDef *dev = (CAN_TypeDef *)CANmodule->CANptr;

    CO_LOCK_CAN_SEND();
    /* Abort message from CAN module, if there is synchronous TPDO.
     * Take special care with this functionality. */
    if (can_is_transmitting(dev) && CANmodule->bufferInhibitFlag)
    {
        /* clear TXREQ */
        can_tx_abort(dev);
        CANmodule->bufferInhibitFlag = false;
        tpdoDeleted = 1U;
    }
    /* delete also pending synchronous TPDOs in TX buffers */
    if (CANmodule->CANtxCount != 0U)
    {
        uint16_t i;
        CO_CANtx_t *buffer = &CANmodule->txArray[0];
        for (i = CANmodule->txSize; i > 0U; i--)
        {
            if (buffer->bufferFull)
            {
                if (buffer->syncFlag)
                {
                    buffer->bufferFull = false;
                    CANmodule->CANtxCount--;
                    tpdoDeleted = 2U;
                }
            }
            buffer++;
        }
    }
    CO_UNLOCK_CAN_SEND();

    if (tpdoDeleted != 0U)
    {
        CANmodule->CANerrorStatus |= CO_CAN_ERRTX_PDO_LATE;
    }
}

/******************************************************************************/
/* Get error counters from the module. If necessary, function may use
    * different way to determine errors. */
static uint16_t rxErrors = 0, txErrors = 0, overflow = 0;

void CO_CANmodule_process(CO_CANmodule_t *CANmodule)
{
    uint32_t err;
    CAN_TypeDef *dev = (CAN_TypeDef *)CANmodule->CANptr;

    rxErrors = can_rx_error_counter(dev);
    txErrors = can_tx_error_counter(dev);
    overflow = can_is_bus_off(dev) ? 1 : 0;

    err = ((uint32_t)txErrors << 16) | ((uint32_t)rxErrors << 8) | overflow;

    if (CANmodule->errOld != err)
    {
        uint16_t status = CANmodule->CANerrorStatus;

        CANmodule->errOld = err;

        if (txErrors >= 256U)
        {
            /* bus off */
            status |= CO_CAN_ERRTX_BUS_OFF;
        }
        else
        {
            /* recalculate CANerrorStatus, first clear some flags */
            status &= 0xFFFF ^ (CO_CAN_ERRTX_BUS_OFF |
                                CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE |
                                CO_CAN_ERRTX_WARNING | CO_CAN_ERRTX_PASSIVE);

            /* rx bus warning or passive */
            if (rxErrors >= 128)
            {
                status |= CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE;
            }
            else if (rxErrors >= 96)
            {
                status |= CO_CAN_ERRRX_WARNING;
            }

            /* tx bus warning or passive */
            if (txErrors >= 128)
            {
                status |= CO_CAN_ERRTX_WARNING | CO_CAN_ERRTX_PASSIVE;
            }
            else if (rxErrors >= 96)
            {
                status |= CO_CAN_ERRTX_WARNING;
            }

            /* if not tx passive clear also overflow */
            if ((status & CO_CAN_ERRTX_PASSIVE) == 0)
            {
                status &= 0xFFFF ^ CO_CAN_ERRTX_OVERFLOW;
            }
        }

        if (overflow != 0)
        {
            /* CAN RX bus overflow */
            status |= CO_CAN_ERRRX_OVERFLOW;
        }

        CANmodule->CANerrorStatus = status;
    }
}

/******************************************************************************/

void CO_CANinterrupt(CO_CANmodule_t *CANmodule)
{
    CAN_TypeDef *dev = (CAN_TypeDef *)CANmodule->CANptr;
    uint32_t txMask = 0;
    /* receive interrupt */
    if (can_is_rx_pending(dev))
    {
        CO_CANrxMsg_t *rcvMsg;     /* pointer to received message in CAN module */
        uint16_t index;            /* index of received message */
        uint32_t rcvMsgIdent;      /* identifier of the received message */
        uint32_t rcvMsgIdentDebug; /* identifier of the received message in 11 bit format */
        CO_CANrx_t *buffer = NULL; /* receive message buffer from CO_CANmodule_t object. */
        bool_t msgMatched = false;
        uint32_t nbrOfRxMsg = 0;

        index = can_rx_get_filter_index(dev); /* get index of the received message here. Or something similar */
        nbrOfRxMsg = can_nbr_of_rx_pending(dev);
        
        rcvMsg->DLC = can_rx_get_dlc(dev);
        rcvMsg->ident = can_rx_get_ident(dev);
        can_rx_get_data(dev, rcvMsg->data);
        can_release_rx_message(dev);
        
        rcvMsgIdent = rcvMsg->ident;
        rcvMsgIdentDebug = (CAN_RI0R_STID & rcvMsg->ident) >> CAN_TI0R_STID_Pos;

        if (CANmodule->useCANrxFilters)
        {
            /* CAN module filters are used. Message with known 11-bit identifier has */
            /* been received */
            //index = can_rx_get_filter_index(dev); /* get index of the received message here. Or something similar */
            if (index < CANmodule->rxSize)
            {
                buffer = &CANmodule->rxArray[index];
                /* verify also RTR */
                if (((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U)
                {
                    msgMatched = true;
                }
            }
        }
        else
        {
            /* CAN module filters are not used, message with any standard 11-bit identifier */
            /* has been received. Search rxArray form CANmodule for the same CAN-ID. */
            buffer = &CANmodule->rxArray[0];
            for (index = CANmodule->rxSize; index > 0U; index--)
            {
                if (((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U)
                {
                    msgMatched = true;
                    break;
                }
                buffer++;
            }
        }

        /* Call specific function, which will process the message */
        if (msgMatched && (buffer != NULL) && (buffer->CANrx_callback != NULL))
        {
            buffer->CANrx_callback(buffer->object, (void *)rcvMsg);
        }

        /* Clear interrupt flag */
    }

    /* transmit interrupt */
    else if ((txMask = can_is_message_sent(dev)))
    {
        /* Clear interrupt flag */
        can_release_tx_message(dev, txMask);

        /* First CAN message (bootup) was sent successfully */
        CANmodule->firstCANtxMessage = false;
        /* clear flag from previous message */
        CANmodule->bufferInhibitFlag = false;
        /* Are there any new messages waiting to be send */
        if (CANmodule->CANtxCount > 0U)
        {
            uint16_t i; /* index of transmitting message */

            /* first buffer */
            CO_CANtx_t *buffer = &CANmodule->txArray[0];
            /* search through whole array of pointers to transmit message buffers. */
            for (i = CANmodule->txSize; i > 0U; i--)
            {
                /* if message buffer is full, send it. */
                if (buffer->bufferFull)
                {
                    buffer->bufferFull = false;
                    CANmodule->CANtxCount--;

                    /* Copy message to CAN buffer */
                    CANmodule->bufferInhibitFlag = buffer->syncFlag;
                    /* canSend... */
                    can_tx(dev, buffer->ident, buffer->DLC, buffer->data);
                    break; /* exit for loop */
                }
                buffer++;
            } /* end of for loop */

            /* Clear counter if no more messages */
            if (i == 0U)
            {
                CANmodule->CANtxCount = 0U;
            }
        }
    }
    else
    {
        /* some other interrupt reason */
    }
}
