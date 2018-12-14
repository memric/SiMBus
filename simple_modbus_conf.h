/*
 * simple_modbus_conf.h
 *
 * Simple ModBus RTU Configuretion file
 *
 *  Created on: 28 нояб. 2018 г.
 *      Author: valeriychudnikov
 */

//#include "stm32l0xx_hal.h"

#ifndef SIMPLE_MODBUS_CONF_H_
#define SIMPLE_MODBUS_CONF_H_

#define MODBUS_COILS_ENABLE		0	//Enable coils read/write
#define MODBUS_DINP_ENABLE		0	//Enable Discrete Inputs read
#define MODBUS_REGS_ENABLE		1	//Enable registers
#define MODBUS_WRMCOILS_ENABLE	0	//Enable multiple coils write

#define MODBUS_TRACE_ENABLE 		0	//Enable Trace

#if MODBUS_TRACE_ENABLE
#define MBRTU_TRACE printf
#else
#define MBRTU_TRACE(message...)
#endif

#endif /* SIMPLE_MODBUS_CONF_H_ */
