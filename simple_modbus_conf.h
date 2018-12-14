/*
 * simple_modbus_conf.h
 *
 * Simple ModBus RTU Configuretion file
 *
 *  Created on: 28 нояб. 2018 г.
 *      Author: valeriychudnikov
 */

#ifndef SIMPLE_MODBUS_CONF_H_
#define SIMPLE_MODBUS_CONF_H_

#define MODBUS_COILS_ENABLE		1	//Enable coils read/write
#define MODBUS_DINP_ENABLE		1	//Enable Discrete Inputs read
#define MODBUS_REGS_ENABLE		1	//Enable registers
#define MODBUS_WRMCOILS_ENABLE	1	//Enable multiple coils write

#define MODBUS_TRACE_ENABLE 		0	//Enable Trace

#if MODBUS_TRACE_ENABLE
#define MBRTU_TRACE printf
#else
#define MBRTU_TRACE
#endif

#endif /* SIMPLE_MODBUS_CONF_H_ */
