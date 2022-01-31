/*
 * simple_modbus.c
 *
 *  Created on: 2016
 *      Author: Valeriy Chudnikov
 */

#include "mbrtu.h"
#include "string.h"
#include "mb_crc.h"

#define MODBUS_FUNC_RDCOIL 		1 	/*Read Coil*/
#define MODBUS_FUNC_RDDINP 		2 	/*Read discrete input*/
#define MODBUS_FUNC_RDHLDREGS 	3	/*Read holding register*/
#define MODBUS_FUNC_RDINREGS  	4 	/*Read input register*/
#define MODBUS_FUNC_WRSCOIL  	5 	/*Write single coil*/
#define MODBUS_FUNC_WRSREG  	6 	/*Write single register*/
#define MODBUS_FUNC_WRMCOILS 	15  /*Write multiple coils*/
#define MODBUS_FUNC_WRMREGS 	16  /*Write multiple registers*/

#define MODBUS_MSG_MIN_LEN		6	/*Minimal message length (addr + func + strt addr + CRC)*/

#define DE_HIGH()				mb->set_de(DEHIGH)
#define DE_LOW()				mb->set_de(DELOW)

static void MBRTU_Parser(smpl_modbus_t *mb);
static void MBRTU_SendException(smpl_modbus_t *mb, MBerror excpt);
static void MBRTU_LolevelSend(smpl_modbus_t *mb, uint32_t len);
#if MODBUS_REGS_ENABLE
extern MBerror MBRegInit(void *arg);
extern MBerror MBRegReadCallback(uint16_t addr, uint16_t num, uint16_t **regs);
extern MBerror MBRegWriteCallback(uint16_t addr, uint16_t val);
#endif
#if MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE
extern MBerror MBCoilInpReadCallback(uint16_t addr, uint16_t num, uint8_t **coils);
extern MBerror MBCoilsInputsWriteCallback(uint16_t addr, uint16_t num, uint8_t *coils);
extern MBerror MBCoilWriteCallback(uint16_t addr, uint8_t val);
#endif

MBerror MBRTU_Init(smpl_modbus_t *mb)
{
	MB_ASSERT(mb != NULL);
	MB_ASSERT(mb->addr > 0);
	MB_ASSERT(mb->rx_buf_len > 0);
	MB_ASSERT(mb->tx_func != NULL);
	MB_ASSERT(mb->rx_func != NULL);
	MB_ASSERT(mb->rx_stop != NULL);
	MB_ASSERT(mb->set_de != NULL);
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
 * @param mb
 */
void MBRTU_Poll(smpl_modbus_t *mb)
{
	MB_ASSERT(mb != NULL);

	if (mb->mbmode == RX)
	{
		if ((mb->rx_byte > mb->rx_buf) && (MODBUS_GET_TICK - mb->last_rx_byte_time > MODBUS_RXWAIT_TIME))
		{
			mb->rx_stop();
			mb->mbmode = TX;

			/*Check message minimal length*/
			if ((mb->rx_byte -  mb->rx_buf) > MODBUS_MSG_MIN_LEN)
			{
				/*Parse incoming message*/
				MBRTU_Parser(mb);
			}

			mb->rx_byte = mb->rx_buf;
			mb->mbmode = RX;
			mb->rx_func(mb->rx_byte);
		}
	}
}

static void MBRTU_Parser(smpl_modbus_t *mb)
{
	/* Packet structure
	 * 8bit - Address
	 * 8bit - Function
	 * nx8bit - Data
	 * 16bit - CRC
	 */

	uint8_t func;
	uint16_t start_addr;
	uint16_t points_num;
	uint16_t tmp_crc;
	uint16_t val;
	MBerror err = MODBUS_ERR_OK;
#if MODBUS_WRMREGS_ENABLE
	uint32_t byte_cnt;
#endif

	/*Check address first*/
	if (mb->rx_buf[0] == mb->addr)
	{
		func = mb->rx_buf[1];
		start_addr = ARR2U16(&mb->rx_buf[2]);
		points_num = ARR2U16(&mb->rx_buf[4]);
		tmp_crc = ARR2U16(&mb->rx_buf[6]);

		/*check function*/
		switch (func)
		{
		/*Function 01: read coil status*/
		case MODBUS_FUNC_RDCOIL:
		/*Function 02: read input status*/
		case MODBUS_FUNC_RDDINP:
#if MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 6))
			{
				uint8_t *coil_values;

				if (points_num >= 1 || points_num <= 2000)
				{
					/*coils/dinputs read callback*/
					err = MBCoilInpReadCallback(start_addr, points_num, &coil_values);
				}
				else
				{
					err = MODBUS_ERR_ILLEGVAL;
				}

				if (err)
				{
					/*send exception*/
					MBRTU_SendException(mb, err);
					break;
				}
				else
				{
					mb->tx_buf[0] = mb->rx_buf[0]; /*copy address*/
					mb->tx_buf[1] = mb->rx_buf[1]; /*copy function*/
					mb->tx_buf[2] = (points_num + 7) / 8; //bytes number
					for (int i = 0; i < mb->tx_buf[2]; i++)
					{
						mb->tx_buf[3 + i] = coil_values[i];
					}
					tmp_crc = ModRTU_CRC(mb->tx_buf, 3 + mb->tx_buf[2]);
					U162ARR(tmp_crc, &mb->tx_buf[3 + mb->tx_buf[2]]);
				}

				MBRTU_LolevelSend(mb, 3 + mb->tx_buf[2] + 2);
			}
#else
			MBRTU_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE*/
			break;

		/*Function 03: read holding registers*/
		case MODBUS_FUNC_RDHLDREGS:
		/*Function 04: read input registers*/
		case MODBUS_FUNC_RDINREGS:
#if MODBUS_REGS_ENABLE
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 6))
			{
				uint16_t *reg_values;

				if (points_num >= 1 || points_num <= 125)
				{
					/*reg read callback*/
					err = MBRegReadCallback(start_addr, points_num, &reg_values);
				}
				else
				{
					err = MODBUS_ERR_ILLEGVAL;
				}

				if (err)
				{
					/*send exception*/
					MBRTU_SendException(mb, err);
					break;
				}
				else
				{
					mb->tx_buf[0] = mb->rx_buf[0]; /*copy address*/
					mb->tx_buf[1] = mb->rx_buf[1]; /*copy function*/
					mb->tx_buf[2] = points_num * 2;
					for (int i = 0; i < points_num; i++)
					{
						U162ARR(reg_values[i], &mb->tx_buf[3 + 2*i]);
					}
					tmp_crc = ModRTU_CRC(mb->tx_buf, 3 + mb->tx_buf[2]);
					U162ARR(tmp_crc, &mb->tx_buf[3 + mb->tx_buf[2]]);
				}

				MBRTU_LolevelSend(mb, 3 + mb->tx_buf[2] + 2);
			}
#else
			MBRTU_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_REGS_ENABLE*/
			break;

			/*Function 05: force single coil*/
		case MODBUS_FUNC_WRSCOIL:
#if MODBUS_COILS_ENABLE
			val = points_num;
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 6))
			{
				uint8_t c_val = 0;
				if (val)
				{
					if (val == 0xFF00) //coil is ON
						c_val = 1;
					else
					{
						/*send exception*/
						MBRTU_SendException(mb, MODBUS_ERR_ILLEGVAL);
						break;
					}
				}

				/*coil write callback*/
				err = MBCoilWriteCallback(start_addr, c_val);
				if (err)
				{
					/*send exception*/
					MBRTU_SendException(mb, err);
					break;
				}
				else
				{
					memcpy(mb->tx_buf, mb->rx_buf, 8); //response
				}

				MBRTU_LolevelSend(mb, 8);
			}
#else
			MBRTU_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_COILS_ENABLE*/
			break;

		/*Function 06: Preset Single Register*/
		case MODBUS_FUNC_WRSREG:
#if MODBUS_REGS_ENABLE
			val = points_num;
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 6))
			{
				/*reg write callback*/
				err = MBRegWriteCallback(start_addr, val);
				if (err)
				{
					/*send exception*/
					MBRTU_SendException(mb, err);
					break;
				}
				else
				{
					memcpy(mb->tx_buf, mb->rx_buf, 8);
				}

				MBRTU_LolevelSend(mb, 8);
			}
#else
			MBRTU_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_REGS_ENABLE*/
			break;

		case MODBUS_FUNC_WRMCOILS: //write coils
#if MODBUS_COILS_ENABLE && MODBUS_WRMCOILS_ENABLE
			val = (uint16_t) mb->rx_buf[6];
			tmp_crc = ARR2U16(&mb->rx_buf[7]);
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 7+val))
			{
				err = MBCoilsInputsWriteCallback(start_addr, points_num, &mb->rx_buf[7]);
				if (err)
				{
					/*send exception*/
					MBRTU_SendException(mb, err);
					break;
				}
				else
				{
					for (int i = 0; i < 6; i++)
					{
						mb->tx_buf[i] = mb->rx_buf[i];
					}
					tmp_crc = ModRTU_CRC(mb->tx_buf, 6);
					U162ARR(tmp_crc, &mb->tx_buf[6]);
				}

				MBRTU_LolevelSend(mb, 8);
			}
#else
			MBRTU_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif
			break;

		/*Function 16: Write multiple registers*/
		case MODBUS_FUNC_WRMREGS:
#if MODBUS_WRMREGS_ENABLE
			byte_cnt = mb->rx_buf[6];
			if (byte_cnt <= 123*2)
			{
				tmp_crc = ARR2U16(&mb->rx_buf[7 + byte_cnt]);

				if (tmp_crc == ModRTU_CRC(mb->rx_buf, 7 + byte_cnt))
				{
					if (points_num < 1 && points_num > 123)
					{
						err = MODBUS_ERR_ILLEGVAL;
					}
					else
					{
						err = MBRegsWriteCallback(start_addr, points_num, &mb->rx_buf[7]);
					}

					if (err)
					{
						/*send exception*/
						MBRTU_SendException(mb, err);
						break;
					}
					else
					{
						/*Copy Slave address, function, start address, quantity of registers*/
						memcpy(mb->tx_buf, mb->rx_buf, 6);

						tmp_crc = ModRTU_CRC(mb->tx_buf, 6);
						U162ARR(tmp_crc, &mb->tx_buf[6]);
					}

					MBRTU_LolevelSend(mb, 8);
				}
			}
#else
			MBRTU_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif
			break;

			default:
				MBRTU_SendException(mb, MODBUS_ERR_ILLEGFUNC);
				break;
		}
	}
}

/**
 * @brief Sends exception to client
 * @param mb
 * @param func
 * @param excpt
 */
static void MBRTU_SendException(smpl_modbus_t *mb, MBerror excpt)
{
	mb->tx_buf[0] = mb->rx_buf[0]; //address
	mb->tx_buf[1] = mb->rx_buf[1] | 0x80; //function + exception
	mb->tx_buf[2] = excpt; //exception
	uint16_t tmp_crc = ModRTU_CRC(mb->tx_buf, 3);
	U162ARR(tmp_crc, &mb->tx_buf[3]);

	MBRTU_LolevelSend(mb, 5);
}

static void MBRTU_LolevelSend(smpl_modbus_t *mb, uint32_t len)
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

void MBRTU_ByteReceivedCallback(smpl_modbus_t *mb)
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
void MBRTU_tx_cmplt(smpl_modbus_t *mb)
{
	mb->last_tx_time = MODBUS_GET_TICK;
	mb->mbmode = RX;

	DE_LOW();
}
#endif

void MBRTU_error(smpl_modbus_t *mb)
{
	MB_ASSERT(mb != NULL);

	mb->rx_stop();

	mb->rx_byte = mb->rx_buf;
	mb->mbmode = RX;
	mb->rx_func(mb->rx_byte);

	DE_LOW();
}
