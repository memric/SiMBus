/*
 * simple_modbus.h
 *
 *  Created on: 05 ���. 2016 �.
 *      Author: Valeriy Chudnikov
 */

#ifndef SIMPLE_MODBUS_H_
#define SIMPLE_MODBUS_H_

#include <stdint.h>
#include "simple_modbus_conf.h"
#include "mb_regs.h"

/*error codes*/
#define MODBUS_ERR_OK 			0
#define MODBUS_ERR_ILLEGFUNC 	1
#define MODBUS_ERR_ILLEGADDR 	2
#define MODBUS_ERR_ILLEGVAL  	3
#define MODBUS_ERR_PARAM		10
#define MODBUS_ERR_INTFS		11

#define MODBUS_MAX_MSG_LEN 		256

typedef uint8_t MBerror;
enum intfs_mode { RX = 0, TX };
typedef enum {DELOW = 0, DEHIGH} de_state_t;

typedef struct {
	uint8_t addr; /*Slave address*/
	uint8_t *rx_buf; /*Rx buffer*/
	uint8_t *tx_buf; /*Tx buffer*/
	uint8_t *rx_byte;
	uint32_t rx_buf_len;
	uint32_t last_rx_byte_time;
	uint32_t last_tx_time;
	enum intfs_mode mbmode;
	MBerror (*rx_func)(uint8_t *byte); /*Low level rx*/
	MBerror (*tx_func)(uint8_t *data, uint32_t len); /*Low level tx function*/
	void (*rx_stop)(void);
	void (*set_de)(de_state_t s);
} smpl_modbus_t;

MBerror SmplModbus_Start(smpl_modbus_t *mb);
void SmplModbus_Poll(smpl_modbus_t *mb);
void SmplModbus_ByteReceivedCallback(smpl_modbus_t *mb);
#if MODBUS_NONBLOCKING_TX
void SmplModbus_tx_cmplt(smpl_modbus_t *mb); /*Tx has been completed*/
#endif
void SmplModbus_error(smpl_modbus_t *mb); /*Interface error*/

#endif /* SIMPLE_MODBUS_H_ */
