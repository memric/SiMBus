/*
 * MBTCP.h
 *
 *  Created on: 6.05.2020
 *      Author: Valeriy Chudnikov
 */

#ifndef MBTCP_SERVER_H_
#define MBTCP_SERVER_H_

#include "modbus_conf.h"

/**
 * @brief Defines maximum packet size as maximum application data unit (ADU)
 * TCP MODBUS ADU = 253 bytes + MBAP (7 bytes)
 */
#ifndef MBTCP_MAX_PACKET_SIZE
#define MBTCP_MAX_PACKET_SIZE	260
#endif

typedef struct {
        uint8_t unit;                                       /*!< Slave address */
        uint8_t *rx_buf;                                    /*!< Pointer to Rx buffer */
        uint8_t *tx_buf;                                    /*!< Pointer to Tx buffer */
        uint16_t rx_buf_size;                               /*!< Rx buffer size */
        uint16_t tx_buf_size;                               /*!< Tx buffer size */
} MBTCP_Handle_t;

MBerror MBTCP_Init(MBTCP_Handle_t *mbtcp);
void MBTCP_Deinit(void);

#endif /* MBTCP_SERVER_H_ */
