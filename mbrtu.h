/*
 * simple_modbus.h
 *
 *  Created on: 05 ���. 2016 �.
 *      Author: Valeriy Chudnikov
 */

#ifndef SIMPLE_MODBUS_H_
#define SIMPLE_MODBUS_H_

#include "mb_regs.h"
#include "simple_modbus_conf.h"

/*error codes*/
#define MODBUS_ERR_OK			0
#define MODBUS_ERR_ILLEGFUNC	1
#define MODBUS_ERR_ILLEGADDR	2
#define MODBUS_ERR_ILLEGVAL		3
#define MODBUS_ERR_PARAM		10
#define MODBUS_ERR_INTFS		11

#ifndef MBTCP_MAX_PACKET_SIZE
#define MBTCP_MAX_PACKET_SIZE	253 /*Maximum ADU size*/
#endif

#define MODBUS_MAX_MSG_LEN		MBTCP_MAX_PACKET_SIZE

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
#if MODBUS_USE_US_TIMER
	void (*us_sleep)(uint16_t us); /*us timer function pointer*/
#endif
} smpl_modbus_t;

MBerror MBRTU_Init(smpl_modbus_t *mb);
void MBRTU_Poll(smpl_modbus_t *mb);
void MBRTU_ByteReceivedCallback(smpl_modbus_t *mb);
#if MODBUS_NONBLOCKING_TX
void MBRTU_tx_cmplt(smpl_modbus_t *mb); /*Tx has been completed*/
#endif
void MBRTU_error(smpl_modbus_t *mb); /*Interface error*/

#endif /* SIMPLE_MODBUS_H_ */