/*
 * mb_regs.c
 *
 *  Created on: 10 ����. 2017 �.
 *      Author: Valeriy Chudnikov
 */

#include "mb_regs_template.h"
#include "simple_modbus_conf.h"
#include <string.h>

typedef enum {REG_READ, REG_WRITE} RegOpMode;

static uint16_t MBRegs[REG_NUM] = {0};

uint32_t RegCheckOp(uint16_t addr, RegOpMode op);
uint32_t RegCheckVal(uint16_t addr, uint16_t val);

/**
 * @brief Registers initialization. Called on ModBus initialization
 * @param arg
 * @return Error code
 */
MBerror RegInit(void *arg)
{
	(void) arg;

	return MODBUS_ERR_OK;
}

/*Functions 03 & 04 - Read Holding/Input Registers Callback*/
MBerror RegReadCallback(uint16_t addr, uint16_t num, uint16_t **regs)
{
	if ((addr < REG_BASE_ADDR ) || (addr > REG_BASE_ADDR + REG_NUM - 1))
	{
		return MODBUS_ERR_ILLEGADDR;
	}

	if (addr + num > REG_BASE_ADDR + REG_NUM) return MODBUS_ERR_ILLEGADDR;

	if (RegCheckOp(addr, REG_READ))
	{
		*regs = &MBRegs[addr];
	}
	else
	{
		return MODBUS_ERR_ILLEGADDR;
	}


	return MODBUS_ERR_OK;
}

/*Function 06 - Preset Single Register Callback*/
MBerror RegWriteCallback(uint16_t addr, uint16_t val)
{
	if ((addr < REG_BASE_ADDR ) || (addr > REG_BASE_ADDR + REG_NUM - 1))
	{
		return MODBUS_ERR_ILLEGADDR;
	}

	/*Check permission & value*/
	if (RegCheckOp(addr, REG_WRITE))
	{
		if (RegCheckVal(addr, val))
		{
			MBRegs[addr] = val;
			MBRegUpdated(addr, val);
		}
		else
		{
			return MODBUS_ERR_ILLEGVAL;
		}
	}
	else
	{
		return MODBUS_ERR_ILLEGADDR;
	}

	return MODBUS_ERR_OK;
}

/*Function 16 - Preset Multiple Registers Callback*/
MBerror RegsWriteCallback(uint16_t addr, uint8_t *pval, uint16_t num)
{
	uint32_t i;

	if ((addr < REG_BASE_ADDR ) || (addr > REG_BASE_ADDR + REG_NUM - 1))
	{
		return MODBUS_ERR_ILLEGADDR;
	}

	for (i = 0; i < num; i++)
	{
		/*Check permission & value*/
		if (RegCheckOp(addr, REG_WRITE))
		{
			if (RegCheckVal(addr, ARR2U16(pval)))
			{
				MBRegs[addr] = ARR2U16(pval);
				MBRegUpdated(addr, ARR2U16(pval));
			}
			else
			{
				return MODBUS_ERR_ILLEGVAL;
			}
		}
		else
		{
			return MODBUS_ERR_ILLEGADDR;
		}

		addr++;
		pval += 2;
	}

	return MODBUS_ERR_OK;
}

MBerror CoilInpReadCallback(uint16_t addr, uint16_t num, uint8_t **coils)
{
	return MODBUS_ERR_OK;
}

MBerror CoilWriteCallback(uint16_t addr, uint8_t val)
{

	return MODBUS_ERR_OK;
}

/*Application function*/
void RegSetValue(uint16_t addr, uint16_t val, MBerror *err)
{
	if ((addr < REG_BASE_ADDR ) || (addr > REG_BASE_ADDR + REG_NUM - 1))
	{
		*err = MODBUS_ERR_ILLEGADDR;
	}
	else
	{
		*err = MODBUS_ERR_OK;
		MBRegs[addr] = val;
	}
}

/*Application function*/
uint16_t RegGetValue(uint16_t addr, MBerror *err)
{
	if ((addr < REG_BASE_ADDR ) || (addr > REG_BASE_ADDR + REG_NUM - 1))
	{
		*err = MODBUS_ERR_ILLEGADDR;
		return 0;
	}
	else
	{
		*err = MODBUS_ERR_OK;
		return MBRegs[addr];
	}

	return 0;
}

void RestoreRegs(uint16_t *store)
{
	memcpy(MBRegs, store, REG_NUM_TO_FLASH*sizeof(uint16_t));
}

uint8_t *RegsAddr_p8(void)
{
	return (uint8_t *) MBRegs;
}

uint16_t *RegsAddr_p16(void)
{
	return (uint16_t *) MBRegs;
}

/*Check register operation permission*/
uint32_t RegCheckOp(uint16_t addr, RegOpMode op)
{
	if (op == REG_READ)
	{
		return 1;
	}
	else
	{
		if (addr == REG_STATUS) return 0;
		else return 1;
	}

	return 0;
}

/*Check register values restrictions*/
uint32_t RegCheckVal(uint16_t addr, uint16_t val)
{
	if (addr == REG_TEST1 && val < 1) return 0;

	return 1;
}

__weak void MBRegUpdated(uint16_t addr, uint16_t val)
{

}
