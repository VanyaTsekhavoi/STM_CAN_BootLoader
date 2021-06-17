#include "CANopen.h"
#include "301/CO_SDOserver.h"
#include "co_storage.h"
#include "platform_flash.h"
#include "crc32.h"

#include <string.h>

/*
 * platform specific EEPROM functions
 */

extern int __cfg_ldr_start, __cfg_ldr_end;
#define __cfg_ldr_size ((uint32_t)&__cfg_ldr_start) - ((uint32_t)&__cfg_ldr_end)

static void erase_od()
{
	platform_flash_unlock();
	platform_flash_erase_flag_reset();
	platform_flash_erase_range((uint32_t)&__cfg_ldr_start, (uint32_t)__cfg_ldr_size);
	platform_flash_lock();
}

static bool write_od(const CO_OD_entry_t *od, int od_size)
{
	uint32_t data = (uint32_t)&__cfg_ldr_start;
	uint32_t max_ptr = (uint32_t)__cfg_ldr_size - sizeof(uint32_t);
	//skip size field
	uint32_t ptr = sizeof(uint32_t);

	struct
	{
		uint16_t index;
		uint16_t length;
	} ehead;

	platform_flash_unlock();
	platform_flash_erase_flag_reset();

	for(int i = 0; i < od_size; i++)
	{
		if((od[i].attribute & (CO_ODA_MEM_ROM | CO_ODA_WRITEABLE)) 
				!= (CO_ODA_MEM_ROM | CO_ODA_WRITEABLE))
		{
			continue;
		}

		if(od[i].length == 0)
		{
			continue;
		}

		if((ptr + sizeof(ehead) + od[i].length) >= max_ptr)
		{
			platform_flash_lock();
			return false;
		}

		ehead.index = od[i].index;
		ehead.length = od[i].length;
		platform_flash_write(data + ptr, (uint8_t *)&ehead, sizeof(ehead));
		ptr += sizeof(ehead);
		platform_flash_write(data + ptr, od[i].pData, od[i].length);
		ptr += od[i].length;

	}

	//allign at uint32_t boundry
	ptr += (sizeof(uint32_t) - ptr % sizeof(uint32_t));

	if(ptr < max_ptr)
	{
		uint32_t size_used = ptr + sizeof(uint32_t);
		platform_flash_write(data, (uint8_t *)&size_used, sizeof(size_used));
		uint32_t crc = crc32((uint8_t *)data,  ptr);
		platform_flash_write(data + ptr, (uint8_t *)&crc, sizeof(crc));
	}

	platform_flash_lock();
	return true;
}	

void co_storage_read_od(const CO_OD_entry_t *od, int od_size)
{
	uint32_t data = (uint32_t)&__cfg_ldr_start;
	uint32_t max_ptr = (uint32_t)__cfg_ldr_size - sizeof(uint32_t);
	uint32_t ptr = sizeof(uint32_t);
	int od_storage_size = *((uint32_t *)data);

	struct
	{
		uint16_t index;
		uint16_t length;
	} ehead;

	if(od_storage_size >= max_ptr)
	{
		return;
	}

	if(crc32((uint8_t *)data, od_storage_size) != 0)
	{
		return;
	}

	while(ptr < od_storage_size)
	{
		memcpy(&ehead, (void *)(data + ptr), sizeof(ehead));
		ptr += sizeof(ehead);	

		for(int i = 0; i < od_size; i++)
		{
			if((od[i].attribute & (CO_ODA_MEM_ROM | CO_ODA_WRITEABLE)) != 
					(CO_ODA_MEM_ROM | CO_ODA_WRITEABLE))
			{
				continue;
			}
			if(od[i].index != ehead.index)
			{
				continue;
			}
			if(od[i].length != ehead.length)
			{
				continue;
			}

			memcpy(od[i].pData, (void *)(data + ptr), ehead.length);
		}
		ptr += ehead.length;	
	}
}

/*Copies entry from flash into existing OD in RAM
* ret:
*  0 - OK
*  1 - error
*/
uint8_t co_read_entry(const CO_OD_entry_t *od, int od_size, uint16_t index)
{
	uint32_t data = (uint32_t)&__cfg_ldr_start;
	uint32_t max_ptr = (uint32_t)__cfg_ldr_size - sizeof(uint32_t);
	uint32_t ptr = sizeof(uint32_t);
	int od_storage_size = *((uint32_t *)data);
	uint8_t ret = 1;
	struct
	{
		uint16_t index;
		uint16_t length;
	} ehead;
	if (od_storage_size >= max_ptr)
	{
		return;
	}
	if (crc32((uint8_t *)data, od_storage_size) != 0)
	{
		return;
	}
	while (ptr < od_storage_size)
	{
		memcpy(&ehead, (void *)(data + ptr), sizeof(ehead));
		ptr += sizeof(ehead);
		if (ehead.index != index)
			ptr += ehead.length;
		else
		{
			for (int i = 0; i < od_size; i++)
			{
				if (od[i].index != ehead.index)
				{
					continue;
				}
				if ((od[i].attribute & (CO_ODA_MEM_ROM | CO_ODA_WRITEABLE)) !=
					(CO_ODA_MEM_ROM | CO_ODA_WRITEABLE))
				{
					continue;
				}
				if (od[i].length != ehead.length)
				{
					continue;
				}
				memcpy(od[i].pData, (void *)(data + ptr), ehead.length);
				ret = 0;
				break;
			}
			break;
		}
	}
	return ret;
}

static CO_SDO_abortCode_t CO_ODF_1010_StoreParam(CO_ODF_arg_t *ODF_arg)
{
	uint32_t value;
	CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

	value = CO_getUint32(ODF_arg->data);

	if(!ODF_arg->reading)
	{
		/* don't change the old value */
		memcpy(ODF_arg->data, (const uint8_t*)ODF_arg->ODdataStorage, 4U);

		if(ODF_arg->subIndex == 1U)
		{
			if(value == 0x65766173UL)
			{
				CO_SDO_t *SDO = (CO_SDO_t *)ODF_arg->object;
				if(!write_od(SDO->OD, SDO->ODSize))
				{
					ret = CO_SDO_AB_HW;
				}
			}
			else
			{
				ret = CO_SDO_AB_DATA_TRANSF;
			}
		}
	}

	return ret;
}


static CO_SDO_abortCode_t CO_ODF_1011_RestoreParam(CO_ODF_arg_t *ODF_arg)
{
	uint32_t value;
	CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

	value = CO_getUint32(ODF_arg->data);

	if(!ODF_arg->reading)
	{
		/* don't change the old value */
		memcpy(ODF_arg->data, (const uint8_t*)ODF_arg->ODdataStorage, 4U);

		if(ODF_arg->subIndex >= 1U)
		{
			if(value == 0x64616F6CUL)
			{
				erase_od();
			}
			else
			{
				ret = CO_SDO_AB_DATA_TRANSF;
			}
		}
	}

	return ret;
}


void co_storage_init(CO_t* CO)
{
	CO_OD_configure(CO->SDO[0], OD_1010_storeParameters, CO_ODF_1010_StoreParam, (void*)CO->SDO[0], 0, 0);
	CO_OD_configure(CO->SDO[0], OD_1011_restoreDefaultParameters, CO_ODF_1011_RestoreParam, (void *)CO->SDO[0], 0, 0);
}

