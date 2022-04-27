/*
 * mb_regs.h
 *
 * Template file
 */

#ifndef MB_REGS_H_
#define MB_REGS_H_

#include "mb_pdu.h"
#include <stdint.h>

#define REG_OPT_UR					0x01
#define REG_OPT_UWR					0x02
#define REG_OPT_SUR					0x04
#define REG_OPT_SUWR				0x08
#define REG_OPT_R_ONLY				(REG_OPT_UR | REG_OPT_SUR)
#define REG_OPT_WR_ONLY				(REG_OPT_UWR | REG_OPT_SUWR)
#define REG_OPT_UR_SUWR				(REG_OPT_UR | REG_OPT_SUR | REG_OPT_SUWR)
#define REG_OPT_SU_ONLY				(REG_OPT_SUR | REG_OPT_SUWR)
#define REG_OPT_ALL					(REG_OPT_UR | REG_OPT_UWR | REG_OPT_SUR | REG_OPT_SUWR)

/* Register: Reg 1
* Addr: 0x0; Min: 0; Max: 100; Default: 0; Oper: R */
#define REG_STATUS_ADDR				0x0
#define REG_STATUS_MIN				0
#define REG_STATUS_MAX				100
#define REG_STATUS_DEF				0
#define REG_STATUS_OPT				REG_OPT_R_ONLY

/* Register: Reg 2
* Addr: 0x1; Min: 0; Max: 255; Default: 10; Oper: W */
#define REG_VALUE1_ADDR     0x1
#define REG_VALUE1_MIN      0
#define REG_VALUE1_MAX      255
#define REG_VALUE1_DEF      10
#define REG_VALUE1_OPT      REG_OPT_WR_ONLY

/* Register: Reg 3
* Addr: 0x2; Min: 0; Max: 3; Default: 20; Oper: RW */
#define REG_VALUE2_ADDR     0x2
#define REG_VALUE2_MIN      0
#define REG_VALUE2_MAX      3
#define REG_VALUE2_DEF      20
#define REG_VALUE2_OPT      REG_OPT_ALL


#define REG_LAST_ADDR	0x2 /*Last register address*/
#define REG_NUM			3 /*Total registers number*/

/* USER CODE BEGIN */

/* USER CODE END */

#define ARR2U16(a)		(uint16_t) (*(a) << 8) | *( (a)+1 )
#define U162ARR(b,a)	*(a) = (uint8_t) ( (b) >> 8 ); *(a+1) = (uint8_t) ( (b) & 0xff )

MBerror MBRegInit(void *arg);
MBerror MBRegReadCallback(uint16_t addr, uint16_t num, uint16_t **pval);
MBerror MBRegsWriteCallback(uint16_t addr, uint16_t num, uint8_t *pval);
void MBRegSetValue(uint16_t addr, uint16_t val, MBerror *err);
uint16_t MBRegGetValue(uint16_t addr, MBerror *err);
void MBRegUpdated(uint16_t addr, uint16_t val);
void MBRegLock(void);
void MBRegUnlock(void);

#endif /*MB_REGS_H_*/
