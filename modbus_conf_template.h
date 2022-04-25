/*
 * modbus_conf.h
 *
 * Simple ModBus configuration file
 *
 *  Created on: 28.11.2018
 *      Author: Valeriy Chudnikov
 */

#ifndef MODBUS_CONF_H_
#define MODBUS_CONF_H_

#include <stdint.h>
#include <assert.h>

#define MODBUS_COILS_ENABLE		1	/*Enable coils read/write*/
#define MODBUS_DINP_ENABLE		0	/*Enable Discrete Inputs read*/
#define MODBUS_REGS_ENABLE		1	/*Enable registers*/
#define MODBUS_WRMCOILS_ENABLE	0	/*Enable multiple coils write*/

#define MODBUS_TRACE_ENABLE 	0	/*Enable Trace*/
#define MODBUS_NONBLOCKING_TX	0	/*Use non blocking Tx*/
#define MODBUS_RXWAIT_TIME		5

#if MODBUS_TRACE_ENABLE
#define MODBUS_TRACE 			printf
#else
#define MODBUS_TRACE(msg...)
#endif

#ifndef MODBUS_SOFT_DE
#define MODBUS_SOFT_DE			1	/*Enables software DE signal assertion*/
#endif

#ifndef MODBUS_USE_US_TIMER
#define MODBUS_USE_US_TIMER		0	/*us Times usage enabling for DE delay*/
#endif

#ifndef MODBUS_USE_TABLE_CRC
#define MODBUS_USE_TABLE_CRC	0	/*Table CRC calculation usage*/
#endif

#define MODBUS_GET_TICK			HAL_GetTick()

#define MB_ASSERT				assert

typedef uint8_t MBerror;			/*Error type*/

#endif /* MODBUS_CONF_H_ */
