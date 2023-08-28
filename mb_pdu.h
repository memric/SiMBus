/*
 * mb_pdu.h
 *
 *  Created on: 27.04.2022
 *      Author: Valeriy Chudnikov
 */

#ifndef MB_PDU_H_
#define MB_PDU_H_

#include "modbus_conf.h"

/**
 * @brief Modbus exception codes
 */
#define MODBUS_ERR_OK 				0
#define MODBUS_ERR_ILLEGFUNC		1
#define MODBUS_ERR_ILLEGADDR		2
#define MODBUS_ERR_ILLEGVAL			3
/**
 * @brief Additional internal error codes
 * */
#define MODBUS_ERR_SYS				10
#define MODBUS_ERR_INTFS			11

#define ARR2U16(a)					(uint16_t) (*(a) << 8) | *( (a)+1 )
#define U162ARR(b,a)				*(a) = (uint8_t) ( ((b) >> 8) & 0xff ); *(a+1) = (uint8_t) ( (b) & 0xff )

MBerror MB_PDU_Parser(uint8_t *pReqData, uint8_t *pRespData, uint16_t *pRespLen);

#endif /* MB_PDU_H_ */
