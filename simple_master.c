#include "simple_master.h"
#include "mb_crc.h"
#include <string.h>

MBerror SiMasterPDUSend(mb_master_t *mb, uint8_t slave, uint8_t func, uint32_t len);
MBerror SiMasterWaitForResponse(mb_master_t *mb, uint32_t timeout);

MBerror SiMasterInit(mb_master_t *mb)
{
	if (!mb->wait_for_resp && !mb->itfs_write)
	{
		return MODBUS_ERR_MASTER;
	}

	mb->rx_byte = mb->rx_buf;

	MBRTU_TRACE("Starting Modbus Master RTU\r\n");

	return MODBUS_ERR_OK;
}

MBerror SiMasterReadHRegs(mb_master_t *mb, uint8_t slave, uint16_t addr, uint16_t num, uint16_t *val)
{
	MBerror err = MODBUS_ERR_OK;

	if (num < 1 || num > 125) return MODBUS_ERR_VALUE;

	U162ARR(addr, &mb->tx_buf[2]); //starting address
	U162ARR(addr, &mb->tx_buf[4]); //Quantity of registers

	err = SiMasterPDUSend(mb, slave, MODBUS_FUNC_RDHLDREGS, 4);

	if (err != MODBUS_ERR_OK) {
		return err;
	}

	/*wait for response*/
	if (SiMasterWaitForResponse(mb, RESPONSE_TIMEOUT) != MODBUS_ERR_OK) {
		return MODBUS_ERR_TIMEOUT;
	}

	if (mb->rx_buf[0] == slave)
	{
		if (mb->rx_buf[1] == MODBUS_FUNC_RDHLDREGS)
		{
			memcpy((void *) val, (void *) &mb->rx_buf[3], mb->rx_buf[2]); //copy values
			return MODBUS_ERR_OK;
		}

		if (mb->rx_buf[1] & 0x80)
		{
			return mb->rx_buf[1] ^ 0x80; //exception code
		}
	}
	else
	{
		return MODBUS_ERR_VALUE; //may be another error?
	}

	return err;
}

MBerror SiMasterPDUSend(mb_master_t *mb, uint8_t slave, uint8_t func, uint32_t len)
{
	mb->tx_buf[0] = slave; //slave address
	mb->tx_buf[1] = func;  //function code

	uint16_t crc = ModRTU_CRC(mb->tx_buf, len + 1); //CRC for all data

	U162ARR(crc, (uint8_t *) &mb->tx_buf[1] + len);

	return mb->itfs_write(mb->tx_buf, len + 1 + 2);
}

MBerror SiMasterWaitForResponse(mb_master_t *mb, uint32_t timeout)
{
	return mb->wait_for_resp(timeout);
}
