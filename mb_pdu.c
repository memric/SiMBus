/*
 * mb_pdu.h
 *
 *  Created on: 27.04.2022
 *      Author: Valeriy Chudnikov
 */

#include "mb_pdu.h"
#include "mb_regs.h"
#include <string.h>

/**
 * @brief Supported function codes definitions
 */
#define MODBUS_FUNC_RDCOIL 		1 	/*Read Coil*/
#define MODBUS_FUNC_RDDINP 		2 	/*Read discrete input*/
#define MODBUS_FUNC_RDHLDREGS 	3	/*Read holding register*/
#define MODBUS_FUNC_RDINREGS  	4 	/*Read input register*/
#define MODBUS_FUNC_WRSCOIL  	5 	/*Write single coil*/
#define MODBUS_FUNC_WRSREG  	6 	/*Write single register*/
#define MODBUS_FUNC_WRMCOILS 	15  /*Write multiple coils*/
#define MODBUS_FUNC_WRMREGS 	16  /*Write multiple registers*/

#if MODBUS_REGS_ENABLE
extern MBerror MBRegInit(void *arg);
extern MBerror MBRegReadCallback(uint16_t addr, uint16_t num, uint16_t **regs);
extern MBerror MBRegWriteCallback(uint16_t addr, uint16_t val);
#endif /*MODBUS_REGS_ENABLE*/
#if MODBUS_COILS_ENABLE
extern MBerror MBCoilsReadCallback(uint16_t addr, uint16_t num, uint8_t **coils);
extern MBerror MBCoilWriteCallback(uint16_t addr, uint8_t val); //TODO Combine with next function
extern MBerror MBCoilsWriteCallback(uint16_t addr, uint16_t num, uint8_t *coils);
#endif /*MODBUS_COILS_ENABLE*/
#if MODBUS_DINP_ENABLE
extern MBerror MBInputsReadCallback(uint16_t addr, uint16_t num, uint8_t **coils);
#endif /*MODBUS_DINP_ENABLE*/

/**
 * @brief               Parser for Modbus PDU data (consists of Function code
 *                      and function data). Also writes response data.
 * @param pReqData      Pointer to request message
 * @param pRespData     Pointer to response message
 * @param pRespLen      Pointer to response length
 * @return              Exception code
 */
MBerror MB_PDU_Parser(uint8_t *pReqData, uint8_t *pRespData, uint16_t *pRespLen)
{
    MB_ASSERT(pReqData != NULL);
    MB_ASSERT(pRespData != NULL);
    MB_ASSERT(pRespLen != NULL);

    MBerror err = MODBUS_ERR_OK;
    *pRespLen = 0;

    /*--PDU---
     * 1byte - Function code
     * N bytes - Data
     */

    uint8_t fcode = pReqData[0];                /* Function code */
    uint8_t *pdata = &pReqData[1];              /* PDU data */
    uint16_t start_addr = ARR2U16(pdata);       /* start address */
    uint16_t points_num = ARR2U16(pdata + 2);   /* number of coils/regs */

    /*check function*/
    switch (fcode)
    {
#if MODBUS_COILS_ENABLE
        /* Function 01: read coil status */
        case MODBUS_FUNC_RDCOIL:
#endif /*MODBUS_COILS_ENABLE*/

#if MODBUS_DINP_ENABLE
        /* Function 02: read input status */
        case MODBUS_FUNC_RDDINP:
#endif /*MODBUS_DINP_ENABLE*/

#if MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE
        {
            uint8_t *resp_values;

            if (points_num >= 1 && points_num <= 2000)
            {
#if MODBUS_COILS_ENABLE
                if (fcode == MODBUS_FUNC_RDCOIL)
                {
                    /*coils read callback*/
                    err = MBCoilsReadCallback(start_addr, points_num, &resp_values);
                }
#endif /*MODBUS_COILS_ENABLE*/

#if MODBUS_DINP_ENABLE
				if (fcode == MODBUS_FUNC_RDDINP)
				{
					/*dinputs read callback*/
					err = MBInputsReadCallback(start_addr, points_num, &resp_values);
				}
#endif /*MODBUS_DINP_ENABLE*/
            }
            else
            {
                /*Send exception 03*/
                err = MODBUS_ERR_ILLEGVAL;
            }

            if (err == MODBUS_ERR_OK)
            {
                /*Prepare response PDU message*/
                uint8_t resp_bytes = (uint8_t) ((points_num + 7) / 8); /*response bytes number*/

                pRespData[0] = fcode;
                pRespData[1] = resp_bytes;

                uint16_t i;
                for (i = 0; i < resp_bytes; i++)
                {
                    pRespData[2 + i] = resp_values[i];
                }

                *pRespLen = resp_bytes + 2;
            }
        }
            break;
#endif /*MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE*/

#if MODBUS_REGS_ENABLE
	/* Function 03: read holding registers */
	case MODBUS_FUNC_RDHLDREGS:
	/* Function 04: read input registers */
	case MODBUS_FUNC_RDINREGS:
		{
			uint16_t *reg_values;

			if (points_num >= 1 && points_num <= 125)
			{
				/*reg read callback*/
				err = MBRegReadCallback(start_addr, points_num, &reg_values);
			}
			else
			{
				/*Send exception 03*/
				err = MODBUS_ERR_ILLEGVAL;
			}

			if (err == MODBUS_ERR_OK)
			{
				/*Prepare response PDU message*/
				uint16_t resp_bytes = points_num * 2; /*response bytes number*/

				pRespData[0] = fcode;
				pRespData[1] = resp_bytes;

				uint16_t i;
				for (i = 0; i < points_num; i++)
				{
					U162ARR(reg_values[i], &pRespData[2 + 2*i]);
				}

				*pRespLen = resp_bytes + 2;
			}
		}
		break;
#endif /*MODBUS_REGS_ENABLE*/

#if MODBUS_COILS_ENABLE
        /* Function 05: force single coil */
        case MODBUS_FUNC_WRSCOIL:
        {
            uint16_t val = points_num;
            uint8_t c_val = 0;

            if (val)
            {
                if (val == 0xFF00)
                {
                    /* coil is ON */
                    c_val = 1;
                }
                else
                {
                    /*Send exception 03*/
                    err = MODBUS_ERR_ILLEGVAL;
                    break;
                }
            }

            /*coil write callback*/
            err = MBCoilsWriteCallback(start_addr, 1, &c_val);

            if (err == MODBUS_ERR_OK)
            {
                memcpy(pRespData, pReqData, 5);
                *pRespLen = 5;
            }
        }
        break;
#endif /*MODBUS_COILS_ENABLE*/

#if MODBUS_WRREG_ENABLE
        /* Function 06: Preset Single Register */
        case MODBUS_FUNC_WRSREG:
        {
            /*reg write callback*/
            err = MBRegsWriteCallback(start_addr, 1, &pdata[2]);

            if (err == MODBUS_ERR_OK)
            {
                memcpy(pRespData, pReqData, 5);
                *pRespLen = 5;
            }
        }
        break;
#endif /*MODBUS_REGS_ENABLE*/

#if MODBUS_COILS_ENABLE && MODBUS_WRMCOILS_ENABLE
        /* Function 15: Write Multiple Coils */
        case MODBUS_FUNC_WRMCOILS:
        {
            uint8_t byte_cnt = pdata[4];

            if ((points_num >= 1 && points_num <= 1968) && ((points_num + 7) / 8 == byte_cnt))
            {
                err = MBCoilsWriteCallback(start_addr, points_num, &pdata[5]);

                if (err == MODBUS_ERR_OK)
                {
                    memcpy(pRespData, pReqData, 5);
                    *pRespLen = 5;
                }
            }
            else
            {
                /*Send exception 03*/
                err = MODBUS_ERR_ILLEGVAL;
            }
        }
        break;
#endif /*MODBUS_COILS_ENABLE && MODBUS_WRMCOILS_ENABLE*/

#if MODBUS_WRMREGS_ENABLE
        /* Function 16: Write multiple registers */
        case MODBUS_FUNC_WRMREGS:
        {
            uint8_t byte_cnt = pdata[4];

            if ((points_num >= 1 && points_num <= 123) &&
                (byte_cnt == 2*points_num))
            {
                err = MBRegsWriteCallback(start_addr, points_num, &pdata[5]);
            }
            else
            {
                /*Send exception 03*/
                err = MODBUS_ERR_ILLEGVAL;
            }

            if (err == MODBUS_ERR_OK)
            {
                /*Copy function, start address, quantity of registers*/
                memcpy(pReqData, pRespData, 5);
                *pRespLen = 5;
            }
        }
        break;
#endif /*MODBUS_WRMREGS_ENABLE*/

        default:
            /* Send exception 01 (ILLEGAL FUNCTION) */
            err = MODBUS_ERR_ILLEGFUNC;
            break;
    }

    /* Exception response */
    if (err != MODBUS_ERR_OK)
    {
        pRespData[0] |= fcode | 0x80;
        pRespData[1] = err;
        *pRespLen = 2;
    }

    return err;
}
