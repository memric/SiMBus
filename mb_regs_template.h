/*
 * mb_regs.h
 *
 *  Created on: 10 ����. 2017 �.
 *      Author: Valeriy Chudnikov
 */

#ifndef MB_REGS_H_
#define MB_REGS_H_

#include "simple_modbus_conf_template.h"
#include <stdint.h>

#define MODBUS_ERR_OK 			0
#define MODBUS_ERR_ILLEGFUNC 	1
#define MODBUS_ERR_ILLEGADDR 	2
#define MODBUS_ERR_ILLEGVAL  	3
#define MODBUS_ERR_SYS		  	10

#define SET_REG_ADDR(reg, addr)	const uint16_t ##reg_ADDR = (addr)


#define REG_STATUS          0x00 /*System status*/
#define REG_TEST1           0x01 /*Test register 1*/
#define REG_TEST2           0x02 /*Test register 2*/

#define REG_BASE_ADDR       0x00
#define REG_LAST_ADDR		(REG_TEST2)
#define REG_NUM 			(REG_LAST_ADDR - REG_BASE_ADDR + 1)
#define REG_NUM_TO_FLASH 	(REG_SAVE - REG_BASE_ADDR + 1)

#define ARR2U16(a)  		(uint16_t) (*(a) << 8) | *( (a)+1 )
#define U162ARR(b,a)  		*(a) = (uint8_t) ( (b) >> 8 ); *(a+1) = (uint8_t) ( (b) & 0xff )

typedef uint8_t MBerror;

MBerror RegInit(void *arg);
MBerror RegReadCallback(uint16_t addr, uint16_t num, uint16_t **regs);
MBerror RegWriteCallback(uint16_t addr, uint16_t val);
MBerror RegsWriteCallback(uint16_t addr, uint8_t *pval, uint16_t num);
MBerror CoilInpReadCallback(uint16_t addr, uint16_t num, uint8_t **coils);
MBerror CoilWriteCallback(uint16_t addr, uint8_t val);
void RegSetValue(uint16_t addr, uint16_t val, MBerror *err);
uint16_t RegGetValue(uint16_t addr, MBerror *err);
void RestoreRegs(uint16_t *store);
uint8_t *RegsAddr_p8(void);
uint16_t *RegsAddr_p16(void);
void MBRegUpdated(uint16_t addr, uint16_t val);

#endif /* MB_REGS_H_ */
