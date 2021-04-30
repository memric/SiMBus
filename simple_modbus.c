/*
 * simple_modbus.c
 *
 *  Created on: 2016
 *      Author: Valeriy Chudnikov
 */

#include "simple_modbus.h"
#include "string.h"
#include "mb_crc.h"
#if MODBUS_USE_US_TIMER
#include "us_timer.h"
#endif

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

static void SmplModbus_Parser(smpl_modbus_t *mb);
static void SmplModbus_SendException(smpl_modbus_t *mb, MBerror excpt);
static void SmplModbus_LolevelSend(smpl_modbus_t *mb, uint32_t len);
#if MODBUS_REGS_ENABLE
extern MBerror RegReadCallback(uint16_t addr, uint16_t num, uint16_t **regs);
extern MBerror RegWriteCallback(uint16_t addr, uint16_t val);
#endif
#if MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE
extern MBerror CoilInpReadCallback(uint16_t addr, uint16_t num, uint8_t **coils);
extern MBerror CoilsInputsWriteCallback(uint16_t addr, uint16_t num, uint8_t *coils);
extern MBerror CoilWriteCallback(uint16_t addr, uint8_t val);
#endif

MBerror SmplModbus_Start(smpl_modbus_t *mb)
{
	MB_ASSERT(mb != NULL);
	MB_ASSERT(mb->addr > 0);
	MB_ASSERT(mb->rx_buf_len > 0);
	MB_ASSERT(mb->tx_func != NULL);
	MB_ASSERT(mb->rx_func != NULL);
	MB_ASSERT(mb->rx_stop != NULL);
	MB_ASSERT(mb->set_de != NULL);

	mb->rx_byte = mb->rx_buf;
	mb->mbmode = RX;

#if MODBUS_USE_US_TIMER
	if (mb->htim == NULL) {
		return MODBUS_ERR_PARAM;
	}
	us_timer_init();
#endif

	MBRTU_TRACE("Starting Modbus RTU with Address %d\r\n", mb->addr);

	DE_LOW();
	mb->rx_func(mb->rx_byte);

	return MODBUS_ERR_OK;
}

/**
 * @brief Modbus polling function. Checks incoming message.
 * @param mb
 */
void SmplModbus_Poll(smpl_modbus_t *mb)
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
				SmplModbus_Parser(mb);
			}

			mb->rx_byte = mb->rx_buf;
			mb->mbmode = RX;
			mb->rx_func(mb->rx_byte);
		}
	}
}

static void SmplModbus_Parser(smpl_modbus_t *mb)
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
		case MODBUS_FUNC_RDCOIL:	/*Read coils*/
		case MODBUS_FUNC_RDDINP:	/*Read discrete inputs*/
#if MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 6))
			{
				/*coils/dinputs read callback*/
				uint8_t *coil_values;
				err = CoilInpReadCallback(start_addr, points_num, &coil_values);
				if (err)
				{
					/*send exception*/
					SmplModbus_SendException(mb, err);
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

				SmplModbus_LolevelSend(mb, 3 + mb->tx_buf[2] + 2);
			}
#else
			SmplModbus_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE*/
			break;

		case MODBUS_FUNC_RDHLDREGS: /*read holding registers*/
		case MODBUS_FUNC_RDINREGS: /*read input registers*/
#if MODBUS_REGS_ENABLE
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 6))
			{
				/*reg read callback*/
				uint16_t *reg_values;
				err = RegReadCallback(start_addr, points_num, &reg_values);
				if (err)
				{
					/*send exception*/
					SmplModbus_SendException(mb, err);
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

				SmplModbus_LolevelSend(mb, 3 + mb->tx_buf[2] + 2);
			}
#else
			SmplModbus_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_REGS_ENABLE*/
			break;

		case MODBUS_FUNC_WRSCOIL: /*write single coil*/
#if MODBUS_COILS_ENABLE
			val = points_num;
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 6))
			{
				uint8_t c_val = 0;
				if (val)
				{
					if (val == 0xFF00) //coil ON
						c_val = 1;
					else
					{
						/*send exception*/
						SmplModbus_SendException(mb, MODBUS_ERR_ILLEGVAL);
						break;
					}
				}

				/*coil write callback*/
				err = CoilWriteCallback(start_addr, c_val);
				if (err)
				{
					/*send exception*/
					SmplModbus_SendException(mb, func, err);
					break;
				}
				else
				{
					memcpy(mb->tx_buf, mb->rx_buf, 8); //response
				}

				SmplModbus_LolevelSend(mb, 8);
			}
#else
			SmplModbus_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_COILS_ENABLE*/
			break;

		case MODBUS_FUNC_WRSREG: /*write single register*/
#if MODBUS_REGS_ENABLE
			val = points_num;
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 6))
			{
				/*reg write callback*/
				err = RegWriteCallback(start_addr, val);
				if (err)
				{
					/*send exception*/
					SmplModbus_SendException(mb, err);
					break;
				}
				else
				{
					memcpy(mb->tx_buf, mb->rx_buf, 8);
				}

				SmplModbus_LolevelSend(mb, 8);
			}
#else
			SmplModbus_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_REGS_ENABLE*/
			break;

		case MODBUS_FUNC_WRMCOILS: //write coils
#if MODBUS_COILS_ENABLE && MODBUS_WRMCOILS_ENABLE
			val = (uint16_t) mb->rx_buf[6];
			tmp_crc = ARR2U16(&mb->rx_buf[7]);
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 7+val))
			{
				err = CoilsInputsWriteCallback(start_addr, points_num, &mb->rx_buf[7]);
				if (err)
				{
					/*send exception*/
					SmplModbus_SendException(mb, err);
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

				SmplModbus_LolevelSend(mb, 8);
			}
#else
			SmplModbus_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif
			break;

		case MODBUS_FUNC_WRMREGS: /*Write multiple registers*/
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

					err = RegsWriteCallback(start_addr, &mb->rx_buf[7], points_num);

					if (err)
					{
						/*send exception*/
						SmplModbus_SendException(mb, err);
						break;
					}
					else
					{
						/*Copy Slave address, function, start address, quantity of registers*/
						memcpy(mb->tx_buf, mb->rx_buf, 6);

						tmp_crc = ModRTU_CRC(mb->tx_buf, 6);
						U162ARR(tmp_crc, &mb->tx_buf[6]);
					}

					SmplModbus_LolevelSend(mb, 8);
				}
			}
#else
			SmplModbus_SendException(mb, MODBUS_ERR_ILLEGFUNC);
#endif
			break;

			default: break;//SmplModbus_SendException(mb, MODBUS_ERR_ILLEGFUNC);
		}
	}
}

/**
 * @brief Sends exception to client
 * @param mb
 * @param func
 * @param excpt
 */
static void SmplModbus_SendException(smpl_modbus_t *mb, MBerror excpt)
{
	mb->tx_buf[0] = mb->rx_buf[0]; //address
	mb->tx_buf[1] = mb->rx_buf[1] | 0x80; //function + exception
	mb->tx_buf[2] = excpt; //exception
	uint16_t tmp_crc = ModRTU_CRC(mb->tx_buf, 3);
	U162ARR(tmp_crc, &mb->tx_buf[3]);

	SmplModbus_LolevelSend(mb, 5);
}

static void SmplModbus_LolevelSend(smpl_modbus_t *mb, uint32_t len)
{
	DE_HIGH();
#if MODBUS_USE_US_TIMER
	us_timer_start(mb->htim, 100);
#endif

#if MODBUS_NONBLOCKING_TX

#else
	mb->tx_func(mb->tx_buf, len);
#if MODBUS_USE_US_TIMER
	us_timer_start(mb->htim, 100);
#endif
	DE_LOW();
#endif

	mb->last_tx_time = MODBUS_GET_TICK;
}

void SmplModbus_ByteReceivedCallback(smpl_modbus_t *mb)
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
void SmplModbus_tx_cmplt(smpl_modbus_t *mb)
{
	mb->last_tx_time = MODBUS_GET_TICK;
	mb->mbmode = RX;

	DE_LOW();
}
#endif

void SmplModbus_error(smpl_modbus_t *mb)
{
	MB_ASSERT(mb != NULL);

	mb->rx_stop();

	mb->rx_byte = mb->rx_buf;
	mb->mbmode = RX;
	mb->rx_func(mb->rx_byte);

	DE_LOW();
}
