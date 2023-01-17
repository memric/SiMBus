/*
 * mb_rtu.c
 *
 *  Created on: 2016
 *      Author: Valeriy Chudnikov
 */

#include "mbrtu.h"
#include "mb_crc.h"
#include "mb_regs.h"

#define MODBUS_MSG_MIN_LEN		6	/*Minimal message length (addr + func + strt addr + CRC)*/

#if MODBUS_SOFT_DE
#define DE_HIGH()				mb->set_de(DEHIGH)
#define DE_LOW()				mb->set_de(DELOW)
#else
#define DE_HIGH()
#define DE_LOW()
#endif

static void MBRTU_Parser(MBRTU_Handle_t *mb, uint16_t len);
static void MBRTU_SendException(MBRTU_Handle_t *mb, MBerror excpt);
static void MBRTU_LolevelSend(MBRTU_Handle_t *mb, uint32_t len);

/**
 * @brief Initializes Modbus RTU module and starts data reception
 * @param mb Modbus handler
 * @return Error code
 */
MBerror MBRTU_Init(MBRTU_Handle_t *mb)
{
	MB_ASSERT(mb != NULL);
	MB_ASSERT(mb->addr > 0);
	MB_ASSERT(mb->rx_buf_len > 0);
	MB_ASSERT(mb->tx_func != NULL);
	MB_ASSERT(mb->rx_func != NULL);
	MB_ASSERT(mb->rx_stop != NULL);
#if MODBUS_SOFT_DE
	MB_ASSERT(mb->set_de != NULL);
#endif
#if MODBUS_USE_US_TIMER
	MB_ASSERT(mb->us_sleep != NULL);
#endif

	mb->rx_byte = mb->rx_buf;
	mb->mbmode = RX;

	MODBUS_TRACE("Starting Modbus RTU with Address %d\r\n", mb->addr);

	DE_LOW();
	mb->rx_func(mb->rx_byte);

#if MODBUS_REGS_ENABLE
	if (MBRegInit(NULL) != MODBUS_ERR_OK)
	{
		return MODBUS_ERR_SYS;
	}
#endif

	return MODBUS_ERR_OK;
}

/**
 * @brief Modbus polling function. Checks incoming message.
 *        Call this function periodically in loop or thread.
 * @param mb
 */
void MBRTU_Poll(MBRTU_Handle_t *mb)
{
	MB_ASSERT(mb != NULL);

	if (mb->mbmode == RX)
	{
		if ((mb->rx_byte > mb->rx_buf) && (MODBUS_GET_TICK - mb->last_rx_byte_time > MODBUS_RXWAIT_TIME))
		{
			mb->rx_stop();
			mb->mbmode = TX;

			uint16_t rx_len = (uint16_t) (mb->rx_byte -  mb->rx_buf);

			/*Check message minimal length*/
			if (rx_len > MODBUS_MSG_MIN_LEN)
			{
				/*Parse incoming message*/
				MBRTU_Parser(mb, rx_len);
			}

			mb->rx_byte = mb->rx_buf;
			mb->mbmode = RX;
			mb->rx_func(mb->rx_byte);
		}
	}
}

/**
 * @brief Modbus RTU ADU parser
 * @param mb Modbus handle
 */
static void MBRTU_Parser(MBRTU_Handle_t *mb, uint16_t len)
{
	/* Packet ADU structure
	 * N bytes - PDU
	 * 16bit - CRC
	 */

	uint16_t tmp_crc;
	MBerror err = MODBUS_ERR_OK;

	/*Check address first*/
	if (mb->rx_buf[0] == mb->addr)
	{
		uint8_t *pPDU = &mb->rx_buf[1];
		uint8_t *pResp = &mb->tx_buf[1];
		uint16_t resp_len = 0;
		tmp_crc = ARR2U16(&mb->rx_buf[len - 2]);

		/*Check CRC with incoming data*/
		if (tmp_crc == ModRTU_CRC(mb->rx_buf, len - 2))
		{
			err = MB_PDU_Parser(pPDU, pResp, &resp_len);

			if (err == MODBUS_ERR_OK)
			{
				if (resp_len > 0)
				{
					/*Send response*/
					mb->tx_buf[0] = mb->addr;
					tmp_crc = ModRTU_CRC(mb->tx_buf, 1 + resp_len);
					U162ARR(tmp_crc, &mb->tx_buf[1 + resp_len]);

					MBRTU_LolevelSend(mb, 1 + resp_len + 2);
				}
			}
			else
			{
			    MODBUS_TRACE("Function error: %d\r\n", err);

				/*Send exception*/
				MBRTU_SendException(mb, err);
			}
		}
		else
		{
			MODBUS_TRACE("Incorrect CRC\r\n");
		}
	}
}

/**
 * @brief Sends exception to client
 * @param mb Modbus handle
 * @param excpt Exception code
 */
static void MBRTU_SendException(MBRTU_Handle_t *mb, MBerror excpt)
{
	mb->tx_buf[0] = mb->rx_buf[0]; //address
	mb->tx_buf[1] = mb->rx_buf[1] | 0x80; //function + exception
	mb->tx_buf[2] = excpt; //exception
	uint16_t tmp_crc = ModRTU_CRC(mb->tx_buf, 3);
	U162ARR(tmp_crc, &mb->tx_buf[3]);

	MBRTU_LolevelSend(mb, 5);
}

/**
 * @brief Calls low level DE and Tx functions
 * @param mb Modbus handle
 * @param len Message length
 */
static void MBRTU_LolevelSend(MBRTU_Handle_t *mb, uint32_t len)
{
	DE_HIGH();
#if MODBUS_USE_US_TIMER
	mb->us_sleep(100);
#endif

#if MODBUS_NONBLOCKING_TX

#else
	mb->tx_func(mb->tx_buf, len);
#if MODBUS_USE_US_TIMER
	mb->us_sleep(100);
#endif
	DE_LOW();
#endif

	mb->last_tx_time = MODBUS_GET_TICK;
}

/**
 * @brief Call this function on byte reception
 * @param mb Modbus handle
 */
void MBRTU_ByteReceivedCallback(MBRTU_Handle_t *mb)
{
	MB_ASSERT(mb != NULL);

	if (mb->mbmode == RX)
	{
		/*receive next byte*/
		mb->rx_byte++;
		if (mb->rx_byte > &mb->rx_buf[mb->rx_buf_len - 1])
		{
			/* not normal case*/
			mb->rx_byte = mb->rx_buf;
		}

		/*Receive next byte*/
		mb->rx_func(mb->rx_byte);
		mb->last_rx_byte_time = MODBUS_GET_TICK;
	}
}

#if MODBUS_NONBLOCKING_TX
/**
 * @brief Reserved for nonblocking Tx
 * @param mb
 */
void MBRTU_tx_cmplt(MBRTU_Handle_t *mb)
{
	mb->last_tx_time = MODBUS_GET_TICK;
	mb->mbmode = RX;

	DE_LOW();
}
#endif

/**
 * @brief Call this function on UART error case
 * @param mb Modbus handle
 */
void MBRTU_error(MBRTU_Handle_t *mb)
{
	MB_ASSERT(mb != NULL);

	mb->rx_stop();

	mb->rx_byte = mb->rx_buf;
	mb->mbmode = RX;
	mb->rx_func(mb->rx_byte);

	DE_LOW();
}
