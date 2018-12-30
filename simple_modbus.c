/*
 * simple_modbus.c
 *
 *  Created on: 05 ���. 2016 �.
 *      Author: Bamboos
 */

#include "simple_modbus.h"
#include "mb_crc.h"
#include <string.h>

enum intfs_mode { RX, TX };

static uint8_t RxMsg[MODBUS_MAX_MSG_LEN];
static uint8_t TxMsg[MODBUS_MAX_MSG_LEN];
static uint8_t *RxByte;
static uint8_t Addr;
static uint32_t last_rx_byte_time, last_tx_time;
static enum intfs_mode mbmode;

//static void SmplProto_SendError(uint8_t err);
static void SmplModbus_Parser(void);
static void SmplModbus_SendException(uint8_t func, MBerror excpt);
static void SmplModbus_LolevelSend(uint8_t *data, uint32_t len);
extern MBerror RegReadCallback(uint16_t addr, uint16_t num, uint16_t **regs);
extern MBerror RegWriteCallback(uint16_t addr, uint16_t val);
extern MBerror RegMWriteCallback(uint16_t addr, uint16_t num, uint16_t *val);
#if MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE
extern MBerror CoilInpReadCallback(uint16_t addr, uint16_t num, uint8_t **coils);
extern MBerror CoilsInputsWriteCallback(uint16_t addr, uint16_t num, uint8_t *coils);
#endif
#if MODBUS_COILS_ENABLE
extern MBerror CoilWriteCallback(uint16_t addr, uint8_t val);
#endif
extern MBerror RS485_ReceiveByte(uint8_t *b);
extern MBerror RS485_Send(uint8_t *data, uint32_t len);
extern MBerror RS485_AbortReceive(void);
extern uint32_t MB_GetTick(void);

void SmplModbus_Start(uint8_t addr)
{
	/*slave address*/
	Addr = addr;

	RxByte = RxMsg;

	mbmode = RX;

	MBRTU_TRACE("Starting Modbus RTU with Address %d\r\n", Addr);

	RS485_ReceiveByte(RxByte);
}


void SmplModbus_Poll(void)
{
	if (mbmode == RX)
	{
		if ((RxByte > RxMsg) && (MB_GetTick() - last_rx_byte_time > 5))
		{
			mbmode = TX;

			RS485_AbortReceive();

			//HAL_Delay(2); //for silence period

			SmplModbus_Parser();

			RxByte = RxMsg;

			mbmode = RX;

			RS485_ReceiveByte(RxByte);
		}
	}
	else
	{
		/*Modbus in TX mode - NOT NORMAL*/
	}
}

static void SmplModbus_Parser(void)
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

	if (RxMsg[0] == Addr)
	{
		start_addr = ARR2U16(&RxMsg[2]);
		points_num = ARR2U16(&RxMsg[4]);
		tmp_crc = ARR2U16(&RxMsg[6]);

		switch (RxMsg[1]) //check function
		{
		/*Read coils*/
		case MODBUS_FUNC_RDCOIL:
#if MODBUS_COILS_ENABLE == 0
			SmplModbus_SendException(RxMsg[1], MODBUS_ERR_ILLEGFUNC);
			break;
#endif
		/*Read discrete inputs*/
		case MODBUS_FUNC_RDDINP:
#if MODBUS_DINP_ENABLE == 0
			SmplModbus_SendException(RxMsg[1], MODBUS_ERR_ILLEGFUNC);
			break;
#endif
#if MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE
//			start_addr = ARR2U16(&RxMsg[2]);
//			points_num = ARR2U16(&RxMsg[4]);
//			tmp_crc = ARR2U16(&RxMsg[6]);
			//start_addr = (uint16_t) (RxMsg[2] << 8) | RxMsg[3];
			//points_num = (uint16_t) (RxMsg[4] << 8) | RxMsg[5]; //chek for maximum 127!!
			//tmp_crc = (uint16_t) (RxMsg[6] << 8) | RxMsg[7];
			if (tmp_crc == ModRTU_CRC(RxMsg, 6))
			{
				/*coils/dinputs read callback*/
				uint8_t *coil_values;
				MBerror err = CoilInpReadCallback(start_addr, points_num, &coil_values);
				if (err)
				{
					/*send exception*/
					SmplModbus_SendException(RxMsg[1], err);
					//mbmode = RX;
				}
				else
				{
					TxMsg[0] = RxMsg[0];  //copy address
					TxMsg[1] = RxMsg[1];  //copy function
					TxMsg[2] = (points_num + 7) / 8; //bytes number
					for (int i = 0; i < TxMsg[2]; i++)
					{
						TxMsg[3 + i] = coil_values[i];
					}
					tmp_crc = ModRTU_CRC(TxMsg, 3 + TxMsg[2]);
//					TxMsg[3 + TxMsg[2]] = (uint8_t) (tmp_crc >> 8);
//					TxMsg[3 + TxMsg[2] + 1] = (uint8_t) (tmp_crc & 0xff);
					U162ARR(tmp_crc, &TxMsg[3 + TxMsg[2]]);
				}

				SmplModbus_LolevelSend(TxMsg, 3 + TxMsg[2] + 2);
			}
			break;
#endif

#if MODBUS_REGS_ENABLE
		case MODBUS_FUNC_RDHLDREGS:
		case MODBUS_FUNC_RDINREGS: //read registers
			if (tmp_crc == ModRTU_CRC(RxMsg, 6))
			{
				//reg read callback
				uint16_t *reg_values;
				MBerror err = RegReadCallback(start_addr, points_num, &reg_values);
				if (err)
				{
					//send exeption
					SmplModbus_SendException(RxMsg[1], err);
					//mbmode = RX;
				}
				else
				{
					TxMsg[0] = RxMsg[0];
					TxMsg[1] = RxMsg[1];
					TxMsg[2] = points_num * 2;
					for (int i = 0; i < points_num; i++)
					{
						U162ARR(reg_values[i], &TxMsg[3 + 2*i]);
					}
					tmp_crc = ModRTU_CRC(TxMsg, 3 + TxMsg[2]);
					U162ARR(tmp_crc, &TxMsg[3 + TxMsg[2]]);
				}

				SmplModbus_LolevelSend(TxMsg, 3 + TxMsg[2] + 2);
			}
			break;
#endif

#if MODBUS_COILS_ENABLE
		case MODBUS_FUNC_WRSCOIL: //write single coil
//			start_addr = (uint16_t) (RxMsg[2] << 8) | RxMsg[3];
//			val = (uint16_t) (RxMsg[4] << 8) | RxMsg[5];
//			tmp_crc = (uint16_t) (RxMsg[6] << 8) | RxMsg[7];
			val = points_num;
			if (tmp_crc == ModRTU_CRC(RxMsg, 6))
			{
				uint8_t c_val = 0;
				if (val)
				{
					if (val == 0xFF00) //coil ON
						c_val = 1;
					else
					{
						SmplModbus_SendException(RxMsg[1], MODBUS_ERR_ILLEGVAL);
						break;
					}
				}

				//coil write callback
				MBerror err = CoilWriteCallback(start_addr, c_val);
				if (err)
				{
					//send exeption
					SmplModbus_SendException(RxMsg[1], err);
					//mbmode = RX;
				}
				else
				{
					memcpy(TxMsg, RxMsg, 8); //response
				}

				SmplModbus_LolevelSend(TxMsg, 8);
			}
			break;
#endif

		case MODBUS_FUNC_WRSREG: //write single register
//			start_addr = (uint16_t) (RxMsg[2] << 8) | RxMsg[3];
//			val = (uint16_t) (RxMsg[4] << 8) | RxMsg[5];
//			tmp_crc = (uint16_t) (RxMsg[6] << 8) | RxMsg[7];
			val = points_num;
			if (tmp_crc == ModRTU_CRC(RxMsg, 6))
			{
				//reg write callback
				MBerror err = RegWriteCallback(start_addr, val);
				if (err)
				{
					//send exeption
					SmplModbus_SendException(RxMsg[1], err);
					//mbmode = RX;
				}
				else
				{
					memcpy(TxMsg, RxMsg, 8);
				}

				SmplModbus_LolevelSend(TxMsg, 8);
			}
			break;

#if MODBUS_COILS_ENABLE && MODBUS_WRMCOILS_ENABLE
		case MODBUS_FUNC_WRMCOILS: //write coils
//			start_addr = (uint16_t) (RxMsg[2] << 8) | RxMsg[3];
//			points_num = (uint16_t) (RxMsg[4] << 8) | RxMsg[5]; //chek for maximum 127!!
			val = (uint16_t) RxMsg[6];
			tmp_crc = ARR2U16(&RxMsg[7]);
//			tmp_crc = (uint16_t) (RxMsg[7+val] << 8) | RxMsg[8+val];
			if (tmp_crc == ModRTU_CRC(RxMsg, 7+val))
			{
				MBerror err = CoilsInputsWriteCallback(start_addr, points_num, &RxMsg[7]);
				if (err)
				{
					//send exeption
					SmplModbus_SendException(RxMsg[1], err);
				}
				else
				{
					for (int i = 0; i < 6; i++)
					{
						TxMsg[i] = RxMsg[i];
					}
					tmp_crc = ModRTU_CRC(TxMsg, 6);
//					TxMsg[6] = (uint8_t) (tmp_crc >> 8);
//					TxMsg[7] = (uint8_t) (tmp_crc & 0xff);
					U162ARR(tmp_crc, &TxMsg[6]);
				}

				SmplModbus_LolevelSend(TxMsg, 8);
			}
			break;
#endif
		case MODBUS_FUNC_WRMREGS: //write multiple registers
			val = RxMsg[6]; //byte count
			tmp_crc = ARR2U16(&RxMsg[7 + val]);
			if (tmp_crc == ModRTU_CRC(RxMsg, 7 + val))
			{
				MBerror err = RegMWriteCallback(start_addr, points_num, (uint16_t*) &RxMsg[7]);
				if (err)
				{
					//send exeption
					SmplModbus_SendException(RxMsg[1], err);
				}
				else
				{
					for (int i = 0; i < 6; i++)
					{
						TxMsg[i] = RxMsg[i];
					}
					tmp_crc = ModRTU_CRC(TxMsg, 6);
					U162ARR(tmp_crc, &TxMsg[6]);
				}

				SmplModbus_LolevelSend(TxMsg, 8);
			}
			break;

			default: SmplModbus_SendException(RxMsg[1], MODBUS_ERR_ILLEGFUNC);
		}
	}
}

static void SmplModbus_SendException(uint8_t func, MBerror excpt)
{
	TxMsg[0] = RxMsg[0]; //address
	TxMsg[1] = RxMsg[1] | 0x80; //function + exception
	TxMsg[2] = excpt; //exception
	uint16_t tmp_crc = ModRTU_CRC(TxMsg, 3);
	TxMsg[3] = (uint8_t) (tmp_crc >> 8);
	TxMsg[4] = (uint8_t) (tmp_crc & 0xff);

	SmplModbus_LolevelSend(TxMsg, 5);
}

static void SmplModbus_LolevelSend(uint8_t *data, uint32_t len)
{
	RS485_Send(data, len);

	last_tx_time = MB_GetTick();
	//mbmode = RX;
}

void ByteReceivedCallback(void)
{
	if (mbmode == RX)
	{
		/*receive next byte*/
		RxByte++;
		if (RxByte > &RxMsg[MODBUS_MAX_MSG_LEN - 1])
		{
			/* not normal case*/
			RxByte = RxMsg;
		}

		RS485_ReceiveByte(RxByte);
		last_rx_byte_time = MB_GetTick();
	}
}

void DataSentCallback(void)
{
	last_tx_time = MB_GetTick();
	mbmode = RX;
}

void RS485ErrorCallback(void)
{
	RxByte = RxMsg;
	mbmode = RX;
	RS485_ReceiveByte(RxMsg);
}
