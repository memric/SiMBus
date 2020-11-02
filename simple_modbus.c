/*
 * simple_modbus.c
 *
 *  Created on: 2016
 *      Author: Valeriy Chudnikov
 */

#include "simple_modbus.h"
#include "string.h"
#include "main.h"
#include "us_timer.h"
#include "mb_crc.h"

#define MODBUS_FUNC_RDCOIL 		1 	/*Read Coil*/
#define MODBUS_FUNC_RDDINP 		2 	/*Read discrete input*/
#define MODBUS_FUNC_RDHLDREGS 	3	/*Read holding register*/
#define MODBUS_FUNC_RDINREGS  	4 	/*Read input register*/
#define MODBUS_FUNC_WRSCOIL  	5 	/*Write single coil*/
#define MODBUS_FUNC_WRSREG  	6 	/*Write single register*/
#define MODBUS_FUNC_WRMCOILS 	15  /*Write multiple coils*/
#define MODBUS_FUNC_WRMREGS 	16  /*Write multiple registers*/

#define DE_HIGH					mb->set_de(DEHIGH)
#define DE_LOW					mb->set_de(DELOW)

static void SmplModbus_Parser(smpl_modbus_t *mb);
static void SmplModbus_SendException(smpl_modbus_t *mb, uint8_t func, MBerror excpt);
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
	if ((mb->addr == 0) || (mb->tx_func == NULL) || (mb->rx_buf_len == 0) ||
			(mb->rx_func == NULL) || (mb->rx_stop == NULL) ||
			(mb->set_de == NULL) || (mb->htim == NULL)) {
		return MODBUS_ERR_PARAM;
	}

	mb->rx_byte = mb->rx_buf;
	mb->mbmode = RX;

#if MODBUS_USE_US_TIMER
	us_timer_init();
#endif

	MBRTU_TRACE("Starting Modbus RTU with Address %d\r\n", mb->addr);

	mb->rx_func(mb->rx_byte);

	return MODBUS_ERR_OK;
}


void SmplModbus_Poll(smpl_modbus_t *mb)
{
	if (mb->mbmode == RX)
	{
		if ((mb->rx_byte > mb->rx_buf) && (MODBUS_GET_TICK - mb->last_rx_byte_time > MODBUS_RXWAIT_TIME))
		{
			mb->rx_stop();
			mb->mbmode = TX;

			/*Parse incoming message*/
			SmplModbus_Parser(mb);

			mb->rx_byte = mb->rx_buf;
			mb->mbmode = RX;
			mb->rx_func(mb->rx_byte);
		}
	}
	else
	{
		/*Modbus in TX mode - NOT NORMAL*/
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

	uint16_t start_addr;
	uint16_t points_num;
	uint16_t tmp_crc;
	uint16_t val;
	uint32_t byte_cnt;
	MBerror err = MODBUS_ERR_OK;

	if (mb->rx_buf[0] == mb->addr)
	{
		start_addr = ARR2U16(&mb->rx_buf[2]);
		points_num = ARR2U16(&mb->rx_buf[4]);
		tmp_crc = ARR2U16(&mb->rx_buf[6]);

		switch (mb->rx_buf[1]) //check function
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
					SmplModbus_SendException(mb->rx_buf[1], err);
					//mbmode = RX;
				}
				else
				{
					mb->tx_buf[0] = mb->rx_buf[0];  //copy address
					mb->tx_buf[1] = mb->rx_buf[1];  //copy function
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
			break;
#endif /*MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE*/

#if MODBUS_REGS_ENABLE
		case MODBUS_FUNC_RDHLDREGS:
		case MODBUS_FUNC_RDINREGS:	/*read registers*/
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 6))
			{
				//reg read callback
				uint16_t *reg_values;
				err = RegReadCallback(start_addr, points_num, &reg_values);
				if (err)
				{
					//send exeption
					SmplModbus_SendException(mb, mb->rx_buf[1], err);
					//mbmode = RX;
				}
				else
				{
					mb->tx_buf[0] = mb->rx_buf[0];
					mb->tx_buf[1] = mb->rx_buf[1];
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
			break;
#endif /*MODBUS_REGS_ENABLE*/

#if MODBUS_COILS_ENABLE
		case MODBUS_FUNC_WRSCOIL: //write single coil
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
						SmplModbus_SendException(mb->rx_buf[1], MODBUS_ERR_ILLEGVAL);
						break;
					}
				}

				//coil write callback
				err = CoilWriteCallback(start_addr, c_val);
				if (err)
				{
					//send exeption
					SmplModbus_SendException(mb, mb->rx_buf[1], err);
					//mbmode = RX;
				}
				else
				{
					memcpy(mb->tx_buf, mb->rx_buf, 8); //response
				}

				SmplModbus_LolevelSend(mb, 8);
			}
			break;
#endif /*MODBUS_COILS_ENABLE*/

		case MODBUS_FUNC_WRSREG: //write single register
			val = points_num;
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 6))
			{
				//reg write callback
				err = RegWriteCallback(start_addr, val);
				if (err)
				{
					//send exeption
					SmplModbus_SendException(mb, mb->rx_buf[1], err);
					//mbmode = RX;
				}
				else
				{
					memcpy(mb->tx_buf, mb->rx_buf, 8);
				}

				SmplModbus_LolevelSend(mb, 8);
			}
			break;

#if MODBUS_COILS_ENABLE && MODBUS_WRMCOILS_ENABLE
		case MODBUS_FUNC_WRMCOILS: //write coils
			val = (uint16_t) mb->rx_buf[6];
			tmp_crc = ARR2U16(&mb->rx_buf[7]);
			if (tmp_crc == ModRTU_CRC(mb->rx_buf, 7+val))
			{
				err = CoilsInputsWriteCallback(start_addr, points_num, &mb->rx_buf[7]);
				if (err)
				{
					//send exeption
					SmplModbus_SendException(mb->rx_buf[1], err);
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
			break;
#endif
		/*Write multiple registers*/
		case MODBUS_FUNC_WRMREGS:
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
						/*send exeption*/
						SmplModbus_SendException(mb, mb->rx_buf[1], err);
					}
					else
					{
						mb->tx_buf[0] = mb->rx_buf[0]; /*Slave address*/
						mb->tx_buf[1] = mb->rx_buf[1]; /*Function*/
						mb->tx_buf[2] = mb->rx_buf[2]; /*Starting address*/
						mb->tx_buf[3] = mb->rx_buf[3];
						mb->tx_buf[4] = mb->rx_buf[4]; /*Quantity of Registers*/
						mb->tx_buf[5] = mb->rx_buf[5];

						tmp_crc = ModRTU_CRC(mb->tx_buf, 6);
						U162ARR(tmp_crc, &mb->tx_buf[6]);
					}

					SmplModbus_LolevelSend(mb, 8);
				}
			}
			break;

			default: SmplModbus_SendException(mb, mb->rx_buf[1], MODBUS_ERR_ILLEGFUNC);
		}
	}
}

static void SmplModbus_SendException(smpl_modbus_t *mb, uint8_t func, MBerror excpt)
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
	DE_HIGH;
#if MODBUS_USE_US_TIMER
	us_timer_start(mb->htim, 100);
#endif

#if MODBUS_NONBLOCKING_TX
	//HAL_UART_Transmit_IT(&huart1, data, len);
#else
	mb->tx_func(mb->tx_buf, len);
#if MODBUS_USE_US_TIMER
	us_timer_start(mb->htim, 100);
#endif
	DE_LOW;
#endif

	mb->last_tx_time = MODBUS_GET_TICK;
}

void SmplModbus_ByteReceivedCallback(smpl_modbus_t *mb)
{
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

	DE_LOW;
}
#endif

void SmplModbus_error(smpl_modbus_t *mb)
{
	mb->rx_stop();

	mb->rx_byte = mb->rx_buf;
	mb->mbmode = RX;
	mb->rx_func(mb->rx_byte);

	DE_LOW;
}
