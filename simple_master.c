#include "simple_master.h"
#include "mb_crc.h"
#include <string.h>

MBerror SiMasterPDUSend(mb_master_t *mb, uint8_t slave, uint8_t func, uint32_t len);
MBerror SiMasterCheckException(mb_master_t *mb);
MBerror SiMasterWaitForResponse(mb_master_t *mb, uint32_t timeout);

MBerror SiMasterInit(mb_master_t *mb)
{
	if (!mb->wait_for_resp || !mb->itfs_write || !mb->rx_buf || !mb->tx_buf)
	{
		MBRTU_TRACE("ModBus Master Init Problem\r\n");

		return MODBUS_ERR_MASTER;
	}

	mb->rx_byte = mb->rx_buf;

	MBRTU_TRACE("Starting ModBus Master RTU\r\n");

	return MODBUS_ERR_OK;
}

/* Function 03 (0x03) Read Holding Registers*/
MBerror SiMasterReadHRegs(mb_master_t *mb, uint8_t slave, uint16_t addr, uint16_t num, uint16_t *val)
{
	MBerror err = MODBUS_ERR_OK;

	if (num < 1 || num > 125) return MODBUS_ERR_VALUE;

	U162ARR(addr, &mb->tx_buf[2]); //starting address
	U162ARR(num, &mb->tx_buf[4]); //Quantity of registers

	/*Start receiving*/
	if (mb->itfs_read(3 + num*2 + 2) != MODBUS_ERR_OK)
	{
		return MODBUS_ERR_INTFS;
	}

	/*Send PDU*/
	err = SiMasterPDUSend(mb, slave, MODBUS_FUNC_RDHLDREGS, 4);

	if (err != MODBUS_ERR_OK) {
		return err;
	}

	/*wait for response*/
	if (SiMasterWaitForResponse(mb, MODBUS_RESPONSE_TIMEOUT) != MODBUS_ERR_OK && mb->rx_len == 0) {
		return MODBUS_ERR_TIMEOUT;
	}

	if (mb->rx_buf[0] == slave)
	{
		/*Check for exception*/
		err = SiMasterCheckException(mb);

		if (err != MODBUS_ERR_OK)
		{
			return err;
		}

		if (mb->rx_buf[1] == MODBUS_FUNC_RDHLDREGS)
		{
			/*Check CRC*/
			if (ModRTU_CRC(mb->rx_buf, 3 + num*2) != ARR2U16(&mb->rx_buf[3 + num*2]))
			{
				return MODBUS_ERR_CRC;
			}

			/*Copy values*/
			for (int i = 0; i < num; i++)
			{
				val[i] = ARR2U16(&mb->rx_buf[3 + i*2]);
			}

			return MODBUS_ERR_OK;
		}
	}
	else
	{
		return MODBUS_ERR_VALUE; //may be another error?
	}

	return err;
}

/* Function 06 (0x06) Write Single Register*/
MBerror SiMasterWriteReg(mb_master_t *mb, uint8_t slave, uint16_t addr, uint16_t val)
{
	MBerror err = MODBUS_ERR_OK;
	uint32_t i;

	U162ARR(addr, &mb->tx_buf[2]); //address
	U162ARR(val, &mb->tx_buf[4]); //value

	/*Start receiving*/
	mb->itfs_read(6 + 2);

	/*Send PDU*/
	err = SiMasterPDUSend(mb, slave, MODBUS_FUNC_WRSREG, 4);

	if (err != MODBUS_ERR_OK) {
		return err;
	}

	/*wait for response*/
	if (SiMasterWaitForResponse(mb, MODBUS_RESPONSE_TIMEOUT) != MODBUS_ERR_OK) {
		return MODBUS_ERR_TIMEOUT;
	}

	/*Check for exception*/
	err = SiMasterCheckException(mb);

	if (err != MODBUS_ERR_OK)
	{
		return err;
	}

	/*Check CRC*/
	if (ModRTU_CRC(mb->rx_buf, 6) != ARR2U16(&mb->rx_buf[6]))
	{
		return MODBUS_ERR_CRC;
	}

	/*compare request and response. Must be the same*/
	for (i = 0; i < 6; i++)
	{
		if (mb->rx_buf[i] != mb->tx_buf[i])
		{
			err = MODBUS_ERR_VALUE;
			break;
		}
	}

	return err;
}

/* Function 16 (0x10) Write Multiple Registers*/
MBerror SiMasterWriteMRegs(mb_master_t *mb, uint8_t slave, uint16_t addr, uint16_t num, uint16_t *val)
{
	MBerror err = MODBUS_ERR_OK;
	uint32_t i;

	if (num < 1 || num > 123) return MODBUS_ERR_VALUE;

	U162ARR(addr, &mb->tx_buf[2]); //starting address
	U162ARR(num, &mb->tx_buf[4]); //Quantity of registers
	mb->tx_buf[6] = 2*num; //byte count

	/*copy values to tx buffer*/
	memcpy(&mb->tx_buf[7], val, 2*num);

	/*Start receiving*/
	mb->itfs_read(6 + 2);

	/*Send PDU*/
	err = SiMasterPDUSend(mb, slave, MODBUS_FUNC_WRMREGS, 5 + 2*num);

	if (err != MODBUS_ERR_OK) {
		return err;
	}

	/*wait for response*/
	if (SiMasterWaitForResponse(mb, MODBUS_RESPONSE_TIMEOUT) != MODBUS_ERR_OK) {
		return MODBUS_ERR_TIMEOUT;
	}

	/*Check for exception*/
	err = SiMasterCheckException(mb);

	if (err != MODBUS_ERR_OK)
	{
		return err;
	}

	/*Check CRC*/
	if (ModRTU_CRC(mb->rx_buf, 6) != ARR2U16(&mb->rx_buf[6]))
	{
		return MODBUS_ERR_CRC;
	}

	/*compare first bytes of request and response*/
	for (i = 0; i < 6; i++)
	{
		if (mb->rx_buf[i] != mb->tx_buf[i])
		{
			err = MODBUS_ERR_VALUE;
			break;
		}
	}

	return err;
}

MBerror SiMasterPDUSend(mb_master_t *mb, uint8_t slave, uint8_t func, uint32_t len)
{
	mb->tx_buf[0] = slave; //slave address
	mb->tx_buf[1] = func;  //function code

	uint16_t crc = ModRTU_CRC(mb->tx_buf, len + 2); //CRC for all data

	U162ARR(crc, (uint8_t *) &mb->tx_buf[2] + len);

	return mb->itfs_write(mb->tx_buf, len + 2 + 2);
}

/*Check is it exception in response*/
MBerror SiMasterCheckException(mb_master_t *mb)
{
	if (mb->rx_buf[1] & 0x80)
	{
		/*Check CRC*/
		if (ModRTU_CRC(mb->rx_buf, 3) == ARR2U16(&mb->rx_buf[3]))
		{
			return mb->rx_buf[2]; //exception code
		}
		else
		{
			return MODBUS_ERR_CRC;
		}
	}

	return MODBUS_ERR_OK;
}

MBerror SiMasterWaitForResponse(mb_master_t *mb, uint32_t timeout)
{
	return mb->wait_for_resp(timeout);
}
