/*
 * simple_modbus_conf.h
 *
 * Simple ModBus RTU configuration file
 *
 * Author: Valeriy Chudnikov
 */

#ifndef SIMPLE_MASTER_H_
#define SIMPLE_MASTER_H_

#include "modbus_conf.h"

typedef struct {
	MBerror (*itfs_write)(uint8_t *data, uint32_t len);
	MBerror (*itfs_read)(uint32_t len);
	MBerror (*wait_for_resp)(uint32_t timeout);
	uint8_t *rx_buf;
	uint8_t *rx_byte;
	uint8_t *tx_buf;
	uint8_t mb_slave;
	uint32_t rx_len;
	uint32_t rx_wait_len;
} mb_master_t;

MBerror SiMasterInit(mb_master_t *mb);
MBerror SiMasterReadHRegs(mb_master_t *mb, uint8_t slave, uint16_t addr, uint16_t num, uint16_t *val);
MBerror SiMasterWriteReg(mb_master_t *mb, uint8_t slave, uint16_t addr, uint16_t val);
MBerror SiMasterWriteMRegs(mb_master_t *mb, uint8_t slave, uint16_t addr, uint16_t num, uint16_t *val);

#endif
