/*
 * MBTCP.h
 *
 *  Created on: 6.05.2020
 *      Author: Valeriy Chudnikov
 */

#ifndef MBTCP_SERVER_H_
#define MBTCP_SERVER_H_

#include "simple_modbus_conf.h"

#ifndef MBTCP_MAX_PACKET_SIZE
#define MBTCP_MAX_PACKET_SIZE	253
#endif

typedef struct {
	uint8_t *rx_buf; /*Rx buffer*/
	uint8_t *tx_buf; /*Tx buffer*/
	uint16_t rx_buf_size; /*Rx buffer size*/
	uint16_t tx_buf_size; /*Tx buffer size*/
} MBTCP_Handle_t;

MBerror MBTCP_Init(MBTCP_Handle_t *mbtcp);
void MBTCP_Deinit(void);

#endif /* MBTCP_SERVER_H_ */
