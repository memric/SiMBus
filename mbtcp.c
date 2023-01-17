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
	    struct sockaddr_in client_addr;
	    socklen_t addr_len;

		/* Grab new connection. */
		int r_sock = accept(sock, (struct sockaddr *) &client_addr, &addr_len);

		if (r_sock == -1)
		{
			close(r_sock);
			continue;
		}

#if MODBUS_TRACE_ENABLE
		char str[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(client_addr.sin_addr), str, INET_ADDRSTRLEN);
		MODBUS_TRACE("New connection from %s\r\n", str);
#endif /* MODBUS_TRACE_ENABLE */

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
 * @param data_len Response PDU data length
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
	uint16_t resp_len = 0;

	/*MBAP + function code + start addr + points num*/
	if (inlen < MBAP_SIZE + 1 + 4)
	{
		return 0;
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
		return 0;
	}

	if (mbap.unit_id != mbtcp->unit)
	{
		MODBUS_TRACE("Incorrect unit ID: %d\r\n", mbap.unit_id);
		return 0;
	}

	/*--PDU---*/
	uint8_t *pPDU = &indata[7];
	uint8_t *pResp = &mbtcp->tx_buf[MBAP_SIZE];

	err = MB_PDU_Parser(pPDU, pResp, &resp_len);

	if (resp_len > 0)
	{
	    uint8_t fcode = pPDU[0]; /*Function code*/

	    if (err == MODBUS_ERR_OK)
	    {
	        /*Send response*/
	        outlen = MBTCP_resp_pack(mbtcp, &mbap, fcode, resp_len);
	    }
	    else
	    {
	        MODBUS_TRACE("Function error: %d\r\n", err);

	        /*Send exception*/
	        outlen = MBTCP_exception_pack(mbtcp, &mbap, fcode, err);
	    }
	}

	return outlen;
}
