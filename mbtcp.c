/*
 * MBTCP.c
 *
 *  Created on: 6.05.2020
 *      Author: Valeriy Chudnikov
 */

#include "mbtcp.h"
#include "mb_regs.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/sockets.h"
#include <string.h>

#ifndef MBTCP_SERVER_PORT
#define MBTCP_SERVER_PORT 		502
#endif

#define MODBUS_FUNC_RDCOIL 		1 	/*Read Coil*/
#define MODBUS_FUNC_RDDINP 		2 	/*Read discrete input*/
#define MODBUS_FUNC_RDHLDREGS 	3	/*Read holding register*/
#define MODBUS_FUNC_RDINREGS  	4 	/*Read input register*/
#define MODBUS_FUNC_WRSCOIL  	5 	/*Write single coil*/
#define MODBUS_FUNC_WRSREG  	6 	/*Write single register*/
#define MODBUS_FUNC_WRMCOILS 	15  /*Write multiple coils*/
#define MODBUS_FUNC_WRMREGS 	16  /*Write multiple registers*/

#define MBAP_SIZE 				7
#define EXCEPT_SIZE 			(MBAP_SIZE + 1 + 1) /*MBAP + Func code + Err*/

#define ARR2U16(a)  			(uint16_t) (*(a) << 8) | *( (a)+1 )
#define U162ARR(b,a)  			*(a) = (uint8_t) ( (b) >> 8 ); *(a+1) = (uint8_t) ( (b) & 0xff )

#ifndef MODBUS_TCP_TASK_STACK
#define MODBUS_TCP_TASK_STACK	512
#endif

#ifndef MODBUS_TCP_TASK_PRIORITY
#define MODBUS_TCP_TASK_PRIORITY	(tskIDLE_PRIORITY + 3)
#endif

typedef struct {
	uint16_t tran_id;	/*transaction ID*/
	uint16_t prot_id;	/*protocol ID*/
	uint16_t plen;		/*length*/
	uint8_t unit_id;	/*Unit ID*/
} mbap_t;

static TaskHandle_t hMBTCP_Task = NULL;
#if MODBUS_REGS_ENABLE
extern MBerror RegInit(void *arg);
extern MBerror RegReadCallback(uint16_t addr, uint16_t num, uint16_t **regs);
extern MBerror RegsWriteCallback(uint16_t addr, uint16_t val);
#endif

static void MBTCP_Thread(void *arg);
static uint32_t MBTCP_PacketParser(MBTCP_Handle_t *mbtcp, uint32_t inlen);
static uint16_t MBTCP_exception_pack(MBTCP_Handle_t *mbtcp,
		mbap_t *mbap_header, uint8_t fcode, MBerror err);
static uint16_t MBTCP_resp_pack(MBTCP_Handle_t *mbtcp, mbap_t *mbap_header,
		uint8_t fcode, uint32_t data_len);

/**
 * @brief ModBus TCP initialization
 * @param mbtcp MBTCP Handler
 * @return Error code
 */
MBerror MBTCP_Init(MBTCP_Handle_t *mbtcp)
{
	MB_ASSERT(mbtcp != NULL);
	MB_ASSERT(mbtcp->rx_buf != NULL);
	MB_ASSERT(mbtcp->tx_buf != NULL);
	MB_ASSERT(mbtcp->rx_buf_size > 0);
	MB_ASSERT(mbtcp->tx_buf_size >= EXCEPT_SIZE);

	if (hMBTCP_Task != NULL)
	{
		return MODBUS_ERR_SYS;
	}

	MODBUS_TRACE("TCP Modbus Initialization\r\n");

#if MODBUS_REGS_ENABLE
	if (MBRegInit(NULL) != MODBUS_ERR_OK)
	{
		return MODBUS_ERR_SYS;
	}
#endif

	/* Create Start thread */
	if (xTaskCreate(MBTCP_Thread, "Modbus task", MODBUS_TCP_TASK_STACK, mbtcp, MODBUS_TCP_TASK_PRIORITY, &hMBTCP_Task) != pdPASS)
	{
		MODBUS_TRACE("TCP Modbus Task Initialization failure\r\n");
		return MODBUS_ERR_SYS;
	}

	return MODBUS_ERR_OK;
}

void MBTCP_Deinit(void)
{
	MODBUS_TRACE("Modbus TCP terminating\r\n");

	vTaskDelete(hMBTCP_Task);
	hMBTCP_Task = NULL;
}

/**
 * @brief Main ModBus TCP task
 * @param argument Unused
 */
static void MBTCP_Thread(void *arg)
{
	MBTCP_Handle_t *mbtcp = (MBTCP_Handle_t *) arg;
	int32_t recv_len;

	MODBUS_TRACE("Starting ModBus TCP at port: %d\r\n", MBTCP_SERVER_PORT);

	/*Create new socket*/
	int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1)
	{
		MODBUS_TRACE("ModBus TCP server initialization failure\r\n");
		vTaskDelete(NULL);
	}

	struct sockaddr_in addr;
	/* set up address to connect to */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MBTCP_SERVER_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Bind connection */
	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1)
	{
		MODBUS_TRACE("Can't bind ModBus TCP server to port %d\r\n", MBTCP_SERVER_PORT);
		vTaskDelete(NULL);
	}

	/* Tell connection to go into listening mode. */
	if (listen(sock, 2) == -1)
	{
		MODBUS_TRACE("ModBus TCP server failure\r\n");
	}

	while (1)
	{
		/* Grab new connection. */
		int r_sock = accept(sock, NULL, NULL);

		if (r_sock == -1)
		{
			close(r_sock);
			continue;
		}

		while(1)
		{
			/*receive data*/
			recv_len = recv(r_sock, mbtcp->rx_buf, mbtcp->rx_buf_size, 0);

			if (recv_len < 0)
			{
				break;
			}

			/*Parse incoming packet*/
			uint16_t outlen = MBTCP_PacketParser(mbtcp, recv_len);

			/*Send response*/
			if (outlen > 0)
			{
				/*send response*/
				send(r_sock, mbtcp->tx_buf, outlen, 0);
			}
		}

		close(r_sock);
	}

	close(sock);
	vTaskDelete(NULL);
}

/**
 * @brief Exception message composer
 * @param mbtcp Pointer to MBTCP handler
 * @param mbap_header Pointer to MBAP
 * @param fcode Function code
 * @param err Error code
 * @return Message length
 */
static uint16_t MBTCP_exception_pack(MBTCP_Handle_t *mbtcp, mbap_t *mbap_header,
		uint8_t fcode, MBerror err)
{
	uint8_t *outdata = mbtcp->tx_buf;

	mbap_header->plen = 3;

	U162ARR(mbap_header->tran_id, outdata);
	U162ARR(mbap_header->prot_id, outdata + 2);
	U162ARR(mbap_header->plen, outdata + 4);
	U162ARR(mbap_header->unit_id, outdata + 6);
	*(outdata + 7) = fcode;
	*(outdata + 8) = err;

	return EXCEPT_SIZE;
}

/**
 * @brief Response message composer
 * @param mbtcp Pointer to MBTCP handler
 * @param mbap_header Pointer to MBAP
 * @param fcode Function code
 * @param data_len Response data length
 * @return Response length
 */
static uint16_t MBTCP_resp_pack(MBTCP_Handle_t *mbtcp, mbap_t *mbap_header,
		uint8_t fcode, uint32_t data_len)
{
	MB_ASSERT(mbtcp->tx_buf_size >= MBAP_SIZE + 1 + data_len);

	uint8_t *outdata = mbtcp->tx_buf;

	mbap_header->plen = data_len + 2;

	U162ARR(mbap_header->tran_id, outdata);
	U162ARR(mbap_header->prot_id, outdata + 2);
	U162ARR(mbap_header->plen, outdata + 4);
	*(outdata + 6) = mbap_header->unit_id;
	*(outdata + 7) = fcode;

	return MBAP_SIZE + 1 + data_len;
}

/**
 * @brief Incoming packet parser
 * @param mbtcp
 * @param inlen
 * @return
 */
static uint32_t MBTCP_PacketParser(MBTCP_Handle_t *mbtcp, uint32_t inlen)
{
	uint32_t outlen = 0;
	uint8_t *indata = mbtcp->rx_buf;
	MBerror err;
	uint16_t val;
	uint8_t *resp_data = mbtcp->tx_buf;
	uint16_t *reg_values;

	/*MBAP + function code + start addr + points num*/
	if (inlen < MBAP_SIZE + 1 + 4)
	{
		return outlen;
	}

	/*---MBAP---*/
	mbap_t mbap;
	mbap.tran_id = ARR2U16(indata); /*transaction ID*/
	mbap.prot_id = ARR2U16(indata + 2); /*protocol ID*/
	mbap.plen = ARR2U16(indata + 4); /*length*/
	mbap.unit_id = *(indata + 6); /*Unit ID*/

	if (mbap.prot_id != 0)
	{
		MODBUS_TRACE("Incorrect Protocol ID: %d\r\n", mbap.prot_id);
	}

	/*--PDU---*/
	uint8_t fcode = *(indata + 7); /*Function code*/
	uint8_t *pdata = indata + 8; /*PDU data*/
	uint16_t start_addr = ARR2U16(pdata); /*start address*/
	uint16_t points_num = ARR2U16(pdata + 2); /*number of coils/regs*/

	/*check function*/
	switch (fcode)
	{
	/*Function 01: read coil status*/
	case MODBUS_FUNC_RDCOIL:
	/*Function 02: read input status*/
	case MODBUS_FUNC_RDDINP:
#if MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE
		uint8_t *coil_values;

		if (points_num >= 1 || points_num <= 2000)
		{
			/*coils/dinputs read callback*/
			err = MBCoilInpReadCallback(start_addr, points_num, &coil_values);
		}
		else
		{
			err = MODBUS_ERR_ILLEGVAL;
		}


		if (err)
		{
			/*Prepare exception*/
			outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, err);
		}
		else
		{
			uint16_t resp_bytes = (points_num + 7) / 8; /*response bytes number*/
			outlen = MBTCP_resp_pack(mbtcp, &mbap, fcode, resp_bytes + 1);

			*(resp_data + 8) = (uint8_t) resp_bytes;
			uint16_t coil2write = 0;
			uint16_t i, j;
			for (i = 0; i < resp_bytes; i++)
			{
				*(resp_data + 9 + i) = 0;

				/*write bits*/
				for (j = 0; j < 8; j++)
				{
					if (coil2write < points_num) {
						*(resp_data + 9 + i) |= (coil_values[coil2write] & 0x01) << j;
						coil2write++;
					}
				}
			}
		}
#else
		outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_COILS_ENABLE || MODBUS_DINP_ENABLE*/
		break;

	/*Function 03: read holding registers*/
	case MODBUS_FUNC_RDHLDREGS:
	/*Function 04: read input registers*/
	case MODBUS_FUNC_RDINREGS:
#if MODBUS_REGS_ENABLE
		if (points_num >= 1 || points_num <= 125)
		{
			/*reg read callback*/
			err = MBRegReadCallback(start_addr, points_num, &reg_values);
		}
		else
		{
			err = MODBUS_ERR_ILLEGVAL;
		}

		if (err)
		{
			/*Prepare exception*/
			outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, err);
		}
		else
		{
			uint16_t resp_bytes = points_num * 2; /*response bytes number*/
			outlen = MBTCP_resp_pack(mbtcp, &mbap, fcode, resp_bytes + 1);

			*(resp_data + 8) = (uint8_t) resp_bytes;
			for (int i = 0; i < points_num; i++)
			{
				U162ARR(reg_values[i], resp_data + 9 + 2*i);
			}
		}
#else
		outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_REGS_ENABLE*/
		break;

	/*Function 05: force single coil*/
	case MODBUS_FUNC_WRSCOIL:
#if MODBUS_COILS_ENABLE
		val = ARR2U16(pdata + 2);
		uint8_t c_val = 0;

		if (val)
		{
			if (val == 0xFF00) /*coil is ON*/
				c_val = 1;
			else
			{
				outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, MODBUS_ERR_ILLEGVAL);
				break;
			}
		}

		err = MBCoilWriteCallback(start_addr, c_val);

		if (err)
		{
			/*Prepare exception*/
			outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, err);
		}
		else
		{
			outlen = MBTCP_resp_pack(mbtcp, &mbap, fcode, 4);

			U162ARR(start_addr, resp_data + 8);
			U162ARR(val, resp_data + 10);
		}
#else
		outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_COILS_ENABLE*/
		break;

	/*Function 06: Preset Single Register*/
	case MODBUS_FUNC_WRSREG:
#if MODBUS_REGS_ENABLE
		val = ARR2U16(pdata + 2);

		/*regs write callback*/
		err = MBRegsWriteCallback(start_addr, 1, (uint8_t *) &val);

		if (err)
		{
			/*Prepare exception*/
			outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, err);
		}
		else
		{
			outlen = inlen;
			memcpy(resp_data, indata, outlen);
		}
#else
		outlen = MBTCP_exception_pack(&mbap, fcode, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_REGS_ENABLE*/
		break;

		/*Function 16: Write multiple registers*/
		case MODBUS_FUNC_WRMREGS:
#if MODBUS_WRMREGS_ENABLE
			/*check for points number and byte count*/
			if ((points_num >= 1 && points_num <= 123) && (pdata[4] == 2*points_num))
			{
				/*regs write callback*/
				err = MBRegsWriteCallback(start_addr, points_num, &pdata[5]);
			}
			else
			{
				err = MODBUS_ERR_ILLEGVAL;
			}

			if (err)
			{
				/*Prepare exception*/
				outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, err);
			}
			else
			{
				outlen = inlen;
				memcpy(resp_data, indata, outlen);
			}
#else
			outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, MODBUS_ERR_ILLEGFUNC);
#endif /*MODBUS_WRMREGS_ENABLE*/
			break;

	default:
		outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, MODBUS_ERR_ILLEGFUNC);
	}

	return outlen;
}
