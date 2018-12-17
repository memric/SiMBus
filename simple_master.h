#ifndef SIMPLE_MASTER_H_
#define SIMPLE_MASTER_H_

#include "simple_modbus_conf.h"

#define RX_MAX_LEN 256

typedef struct {
	MBerror (*itfs_write)(uint8_t *data, uint32_t len);
	//MBerror (*itfs_get_byte)(uint8_t *byte);  //reserved
	MBerror (*wait_for_resp)(uint32_t timeout);
	uint8_t rx_buf[MAX_MSG_LEN];
	uint8_t *rx_byte;
	uint8_t tx_buf[MAX_MSG_LEN];
	uint8_t mb_slave;
} mb_master_t;

MBerror SiMasterInit(mb_master_t *mb);
MBerror SiMasterReadHRegs(mb_master_t *mb, uint8_t slave, uint16_t addr, uint16_t num, uint16_t *val);

#endif
