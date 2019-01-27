/*
 * simple_modbus_conf.h
 *
 * Simple ModBus RTU Configuretion file
 *
 *  Created on: 28 нояб. 2018 г.
 *      Author: valeriychudnikov
 */

#include <stdint.h>
#include "platform.h"

#ifndef SIMPLE_MODBUS_CONF_H_
#define SIMPLE_MODBUS_CONF_H_

#define MODBUS_COILS_ENABLE			0	//Enable coils read/write
#define MODBUS_DINP_ENABLE			0	//Enable Discrete Inputs read
#define MODBUS_REGS_ENABLE			1	//Enable registers
#define MODBUS_WRMCOILS_ENABLE		0	//Enable multiple coils write

#define MODBUS_TRACE_ENABLE 		1	//Enable Trace
#define MODBUS_MAX_MSG_LEN 			256
#define MODBUS_RESPONSE_TIMEOUT		100
#define MODBUS_TERMINATING_TIME		10

#if MODBUS_TRACE_ENABLE
#define MBRTU_TRACE PTRACE
#else
#define MBRTU_TRACE(message...)
#endif

#define ARR2U16(a)  (uint16_t) ( (*(a) << 8) | (*((a) + 1)) )
#define U162ARR(b,a)  *(a) = (uint8_t) ( (b) >> 8 ); *(a+1) = (uint8_t) ( (b) & 0xff )

/*Modbus function*/
#define MODBUS_FUNC_RDCOIL 		1 	//Read Coil
#define MODBUS_FUNC_RDDINP 		2 	//Read discrete input
#define MODBUS_FUNC_RDHLDREGS 	3	//Read holding register
#define MODBUS_FUNC_RDINREGS  	4 	//Read input register
#define MODBUS_FUNC_WRSCOIL  	5 	//Write single coil
#define MODBUS_FUNC_WRSREG  	6 	//Write single register
#define MODBUS_FUNC_WRMCOILS 	15  //Write multiple coils
#define MODBUS_FUNC_WRMREGS 	16  //Write multiple registers

/*error codes*/
#define MODBUS_ERR_OK 			0
#define MODBUS_ERR_ILLEGFUNC 	1
#define MODBUS_ERR_ILLEGADDR 	2
#define MODBUS_ERR_ILLEGVAL  	3
#define MODBUS_ERR_TIMEOUT		8
#define MODBUS_ERR_VALUE	  	9
#define MODBUS_ERR_INTFS	  	10
#define MODBUS_ERR_MASTER		11
#define MODBUS_ERR_CRC			12

typedef uint8_t MBerror;

#endif /* SIMPLE_MODBUS_CONF_H_ */
