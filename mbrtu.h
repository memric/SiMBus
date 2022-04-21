/*
 * mb_rtu.h
 *
 *  Created on: 2016
 *      Author: Valeriy Chudnikov
 */

#ifndef MBRTU_H_
#define MBRTU_H_

#include "modbus_conf.h"
#include "mb_regs.h"

/**
 * @brief Defines maximum message size as maximum application data unit (ADU)
 * RS232/RS485 ADU = 253 bytes PDU + Server address (1 byte) + CRC (2 bytes)
 */
#ifndef MBRTU_MAX_MSG_LEN
#define MBRTU_MAX_MSG_LEN		256
#endif

enum intfs_mode { RX = 0, TX };
typedef enum {DELOW = 0, DEHIGH} de_state_t;

/**
 * @brief Modbus RTU Handler structure
 */
typedef struct {
	uint8_t addr;										/*!< Slave address */
	uint8_t *rx_buf;									/*!< Pointer to Rx buffer */
	uint8_t *tx_buf;									/*!< Pointer to Tx buffer */
	uint8_t *rx_byte;									/*!< Pointer to current rx byte */
	uint32_t rx_buf_len;								/*!< Size of rx buffer */
	uint32_t last_rx_byte_time;							/*!< Time of reception last byte */
	uint32_t last_tx_time;								/*!< Time of transmission last byte */
	enum intfs_mode mbmode;								/*!< Current mode (rx/tx) */
	MBerror (*rx_func)(uint8_t *byte);					/*!< Low level rx function pointer */
	MBerror (*tx_func)(uint8_t *data, uint32_t len);	/*!< Low level tx function pointer */
	void (*rx_stop)(void);								/*!< Low level rx stop function pointer */
	void (*set_de)(de_state_t s);						/*!< Low level DE control function pointer */
#if MODBUS_USE_US_TIMER
	void (*us_sleep)(uint16_t us);						/*!< us timer function pointer */
#endif
} MBRTU_Handle_t;

MBerror MBRTU_Init(MBRTU_Handle_t *mb);
void MBRTU_Poll(MBRTU_Handle_t *mb);
void MBRTU_ByteReceivedCallback(MBRTU_Handle_t *mb);
#if MODBUS_NONBLOCKING_TX
void MBRTU_tx_cmplt(MBRTU_Handle_t *mb); /*Tx has been completed*/
#endif
void MBRTU_error(MBRTU_Handle_t *mb); /*Interface error*/

#endif /* MBRTU_H_ */
