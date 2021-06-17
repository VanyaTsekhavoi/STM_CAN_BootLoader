#include "canopen_link.h"
#include "platform_reset.h"
#include "platform_flash.h"
#include "loader.h"
#include "CO_OD.h"
#include "co_storage.h"
#include "systemTimer.h"

static CO_SDO_abortCode_t Program_Transfer(CO_ODF_arg_t *ODF_arg)
{
	/*****  SDO block read  *****/
	uint8_t *programData = ODF_arg->data;
	uint32_t start = app_start();
	uint32_t end = app_end();
	uint32_t size = app_size();

	static uint32_t ptr = 0;
	static uint32_t state = 0;

	HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, RESET);
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, RESET);

	/***** Copying from ODF_arg bufer into Flash *****/

	if (ODF_arg->dataLengthTotal >= size)
	{
		//HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, SET);
		return CO_SDO_AB_DATA_TRANSF;
	}

	switch (state)
	{
	case 0:
		platform_flash_unlock();
		platform_flash_erase_flag_reset();
		OD_verifyApplicationSoftware[0] = 0;
		state++;
	case 1:
		platform_flash_write(start + ptr, programData, ODF_arg->dataLength);
		ptr += ODF_arg->dataLength;

		if (ptr >= ODF_arg->dataLengthTotal)
		{
			HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, SET);
			state = 0;
			platform_flash_lock();
		}
		break;

	default:
		break;
	}

	HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
	return CO_SDO_AB_NONE;
}

static CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
static int32_t clk = 0;
static int32_t process_timer = 0;
static CO_ReturnError_t err;
static uint32_t heapMemoryUsed;
extern sysTick_timer_param_t sysTick;

void canopen_link_init()
{
	/* Configure microcontroller. */
	const int canbr[] = {1000, 800, 500, 250, 125, 100, 50, 20, 10};
	extern const CO_OD_entry_t CO_OD[CO_OD_NoOfElements];

	co_storage_read_od(CO_OD, CO_OD_NoOfElements);

	if (OD_CAN_BitRate > sizeof(canbr) / sizeof(int))
	{
		OD_CAN_BitRate = 0;
	}
	if (CO_init((int32_t)CAN1, OD_CANNodeID, canbr[OD_CAN_BitRate]) != CO_ERROR_NO)
	{
		platform_do_reset();
	}

	/* Configure Timer interrupt function for execution every 1 millisecond */

	/* Configure CAN transmit and receive interrupt */

	/* Configure CANopen callbacks, etc */

	CO_OD_configure(CO->SDO[0], OD_1F50_downloadProgramData, Program_Transfer, (void *)NULL, 0, 0U);
	co_storage_init(CO);

	if (!CO->nodeIdUnconfigured)
	{
	}

	/* start CAN */
	CO_CANsetNormalMode(CO->CANmodule[0]);

	timebase_init();
	timebase_mark(&clk);
	uint32_t sysTickPrescaler = SysTick_CTRL_CLKSOURCE_Msk ? 1 : 8;
	sysTick.clk_mhz = HAL_RCC_GetHCLKFreq() / sysTickPrescaler / 1000000;
	sysTick.ticks_per_ms = HAL_RCC_GetHCLKFreq() / sysTickPrescaler / 1000;
}

void canopen_link_poll()
{
	int32_t dclk = timebase_mark(&clk);
	int32_t delta_us = dclk / sysTick.clk_mhz;
	process_timer += dclk;

	/* CANopen process */
	CO_CANinterrupt(CO->CANmodule[0]);

	if (CO->CANmodule[0]->CANnormal)
	{
		bool_t syncWas;

		/* Process Sync */
		syncWas = CO_process_SYNC(CO, delta_us, NULL);

		/* Read inputs */
		CO_process_RPDO(CO, syncWas);

		/* Further I/O or nonblocking application code may go here. */

		/* Write outputs */
		CO_process_TPDO(CO, syncWas, delta_us, NULL);

		/* verify timer overflow */
		if (0)
		{
			CO_errorReport(CO->em, CO_EM_ISR_TIMER_OVERFLOW, CO_EMC_SOFTWARE_INTERNAL, 0U);
		}
	}

	if (process_timer > sysTick.ticks_per_ms)
	{
		reset = CO_process(CO, process_timer / sysTick.clk_mhz, NULL);
		switch (reset)
		{
		case CO_RESET_APP:
			platform_do_reset();
			break;
		case CO_RESET_COMM:
		{
			CO_delete((int32_t)CAN1);
			canopen_link_init();
		}
		break;
		default:
			break;
		}
		process_timer -= sysTick.ticks_per_ms;
	}
	/* Nonblocking application code may go here. */

	/* Process EEPROM */

	/* optional sleep for short time */
}

CO_ReturnError_t CO_init(
	int32_t CANbaseAddress,
	uint8_t nodeId,
	uint16_t bitRate)
{
	CO_ReturnError_t err;

	/* Allocate memory */
	err = CO_new(&heapMemoryUsed);
	if (err)
	{
		return err;
	}

	/* CANopen communication reset - initialize CANopen objects *******************/
	uint16_t timer1msPrevious;

	/* disable CAN and CAN interrupts */

	/* initialize CANopen */
	err = CO_CANinit(CANbaseAddress, bitRate);
	if (err)
	{
		CO_delete(CANbaseAddress);
		return err;
	}

	/*	Check for SDO TimeOut: should be = 1200	*/
	err = CO_CANopenInit(nodeId);
	if (err)
	{
		CO_delete(CANbaseAddress);
		return err;
	}

	return CO_ERROR_NO;
}
