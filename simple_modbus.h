/*
 * simple_modbus.h
 *
 *  Created on: 05 ���. 2016 �.
 *      Author: Bamboos
 */

#ifndef SIMPLE_MODBUS_H_
#define SIMPLE_MODBUS_H_

#include <stdint.h>

/*error codes*/
#define MODBUS_ERR_OK 			0
#define MODBUS_ERR_ILLEGFUNC 	1
#define MODBUS_ERR_ILLEGADDR 	2
#define MODBUS_ERR_ILLEGVAL  	3

typedef uint8_t MBerror;

void SmplModbus_Start(uint8_t addr);
void SmplModbus_Poll(void);

#endif /* SIMPLE_MODBUS_H_ */
