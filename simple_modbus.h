/*
 * simple_modbus.h
 *
 *  Created on: 05 ���. 2016 �.
 *      Author: Bamboos
 */

#ifndef SIMPLE_MODBUS_H_
#define SIMPLE_MODBUS_H_

#include "simple_modbus_conf.h"

void SmplModbus_Start(uint8_t addr);
void SmplModbus_Poll(void);
void ByteReceivedCallback(void);
void DataSentCallback(void);
void RS485ErrorCallback(void);

#endif /* SIMPLE_MODBUS_H_ */
