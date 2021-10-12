/*
 * MBTCP.c
 *
 *  Created on: 6.05.2020
 *      Author: Valeriy Chudnikov
 */

#include "mbtcp_server.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/api.h"
#include "lwip/netbuf.h"
#include <string.h>

#define MBTCP_SERVER_PORT 		502

#define MODBUS_FUNC_RDCOIL		1 //Read Coil
#define MODBUS_FUNC_RDDINP		2 //Read discrete input
#define MODBUS_FUNC_RDHLDREGS	3 //Read holding register
#define MODBUS_FUNC_RDINREGS	4 //Read input register
#define MODBUS_FUNC_WRSCOIL		5 //Write single coil
#define MODBUS_FUNC_WRSREG		6 //Write single register

#define MODBUS_COILS_EN			1

#define MBAP_SIZE 				7
#define EXCEPT_SIZE 			(MBAP_SIZE + 1 + 1) //MBAP + Func code + Err

#define MBTCP_TRACE 			MODBUS_TRACE
#define MEMORY_MALLOC(a) 		( pvPortMalloc( (a) ) )
#define MEMORY_FREE(a) 			( vPortFree( (a) ) )

#define ARR2U16(a)  			(uint16_t) (*(a) << 8) | *( (a)+1 )
#define U162ARR(b,a)  			*(a) = (uint8_t) ( (b) >> 8 ); *(a+1) = (uint8_t) ( (b) & 0xff )

typedef struct {
	uint16_t tran_id; //transaction ID
	uint16_t prot_id; //protocol ID
	uint16_t plen; //length
	uint8_t unit_id; //Unit ID
} mbap_t;

static TaskHandle_t hMBTCP_Task = NULL;
static struct netconn *conn = NULL;

static void MBTCP_Thread(void *argument);
uint32_t MBTCP_PacketParser(uint8_t *indata, uint32_t inlen, uint8_t **outdata);

int32_t MBTCP_Init(void)
{
	if (hMBTCP_Task != NULL) return 1;

	MODBUS_TRACE("TCP Modbus Initialization\r\n");

	/* Create Start thread */
	if (xTaskCreate(MBTCP_Thread, "Modbus task", 1024*2, NULL, configMAX_PRIORITIES-2, &hMBTCP_Task) != pdPASS)
	{
		return -1;
	}

	return 0;
}

int32_t MBTCP_Deinit(void)
{
	MODBUS_TRACE("Modbus TCP terminating\r\n");

	vTaskDelete(hMBTCP_Task);
	hMBTCP_Task = NULL;
	if (conn)
	{
		netconn_delete(conn);
	}

	return 0;
}

static void MBTCP_Thread(void *argument)
{
	struct netconn *newconn;
	err_t err;

	LWIP_UNUSED_ARG(argument);

	MODBUS_TRACE("Starting Modbus TCP at port: %d\r\n", MBTCP_SERVER_PORT);

	/* Create a new connection identifier. */
	conn = netconn_new(NETCONN_TCP);

	if (conn!=NULL)
	{
		/* Bind connection to port UDP_PORT. */
		err = netconn_bind(conn, IP_ADDR_ANY, MBTCP_SERVER_PORT);

		if (err == ERR_OK)
		{
			/* Tell connection to go into listening mode. */
			netconn_listen(conn);

			while (1)
			{
				/* Grab new connection. */
				err = netconn_accept(conn, &newconn);

				if ( err != ERR_OK )
				{
					continue;
				}

				/* Process the new connection. */
				if (newconn)
				{
					struct netbuf *buf;
					void *data;
					u16_t len;

					while ((err = netconn_recv(newconn, &buf)) == ERR_OK)
					{
						do
						{
							len = netbuf_len(buf);
							data = MEMORY_MALLOC(len);

							if (data != NULL)
							{
								netbuf_copy(buf, data, len);

								//TO DO
								uint8_t *outdata = NULL;
								uint16_t outlen = MBTCP_PacketParser((uint8_t *) data, len, &outdata);
								MEMORY_FREE(data);
								if ((outdata != NULL) && (outlen > 0))
								{
									netconn_write(newconn, outdata, outlen, NETCONN_COPY);
									MEMORY_FREE(outdata);
								}
							}
							//netbuf_data(buf, &data, &len);
							//netconn_write(newconn, data, len, NETCONN_COPY);
						}
						while (netbuf_next(buf) >= 0);

						netbuf_delete(buf);
					}

					/* Close connection and discard connection identifier. */
					netconn_close(newconn);
					netconn_delete(newconn);
				}
			}
		}
		else
		{
			MODBUS_TRACE("Can't to bind MB TCP server to port %d\r\n", MBTCP_SERVER_PORT);
			netconn_delete(conn);
		}
	}
	else
	{
		MODBUS_TRACE("Can't to create MB TCP server\r\n");
	}

	vTaskDelete(NULL);
}

uint8_t* MBTCP_exception_pack(mbap_t *mbap_header, uint8_t fcode, MBerror err, uint32_t *outlen)
{
	uint8_t *outdata= MEMORY_MALLOC(EXCEPT_SIZE);

	if (outdata != NULL)
	{
		mbap_header->plen = 3;

		U162ARR(mbap_header->tran_id, outdata);
		U162ARR(mbap_header->prot_id, outdata + 2);
		U162ARR(mbap_header->plen, outdata + 4);
		U162ARR(mbap_header->unit_id, outdata + 6);
		*(outdata + 7) = fcode;
		*(outdata + 8) = err;

		*outlen = EXCEPT_SIZE;
	}

	return outdata;
}

uint8_t* MBTCP_resp_pack(mbap_t *mbap_header, uint8_t fcode, uint32_t data_len, uint32_t *outlen)
{
	uint8_t *outdata = MEMORY_MALLOC(MBAP_SIZE + 1 + data_len);

	if (outdata != NULL)
	{
		mbap_header->plen = data_len + 2;

		U162ARR(mbap_header->tran_id, outdata);
		U162ARR(mbap_header->prot_id, outdata + 2);
		U162ARR(mbap_header->plen, outdata + 4);
		U162ARR(mbap_header->unit_id, outdata + 6);
		*(outdata + 7) = fcode;

		*outlen = MBAP_SIZE + 1 + data_len;
	}

	return outdata;
}

uint32_t MBTCP_PacketParser(uint8_t *indata, uint32_t inlen, uint8_t **outdata)
{
	uint32_t outlen = 0;

	if (inlen < 8) return outlen;

	/*---MBAP---*/
	mbap_t mbap;
	mbap.tran_id = ARR2U16(indata); //transaction ID
	mbap.prot_id = ARR2U16(indata + 2); //protocol ID
	mbap.plen = ARR2U16(indata + 4); //length
	mbap.unit_id = *(indata + 6); //Unit ID

	if (mbap.prot_id != 0) MBTCP_TRACE("Incorrect Protocol ID: %d\r\n", mbap.prot_id);

	/*--PDU---*/
	uint8_t fcode = *(indata + 7); //Function code
	uint8_t *pdata = indata + 8; //data from here

	uint16_t start_addr; //for address
	uint16_t points_num; //for number of coils/regs
	uint16_t val;
	MBerror err;
	uint8_t *resp_data = NULL;

	/*check function*/
	switch (fcode)
	{
	/*Function 01: read coil status*/
	case MODBUS_FUNC_RDCOIL:
	/*Function 02: read input status*/
	case MODBUS_FUNC_RDDINP:
		start_addr = ARR2U16(pdata);
		points_num = ARR2U16(pdata + 2); //chek for maximum 127!!

		uint8_t *coil_values;
		err = CoilInpReadCallback(start_addr, points_num, &coil_values);

		if (err)
		{
			/*Prepare exception*/
			resp_data = MBTCP_exception_pack(&mbap, fcode, err, &outlen);
		}
		else
		{
			uint16_t resp_bytes = (points_num + 7) / 8; /*response bytes number*/
			resp_data = MBTCP_resp_pack(&mbap, fcode, resp_bytes + 1, &outlen);

			if (resp_data) {
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
		}
		break;

	/*Function 03: read holding registers*/
	case MODBUS_FUNC_RDHLDREGS:
	/*Function 04: read input registers*/
	case MODBUS_FUNC_RDINREGS:
		start_addr = ARR2U16(pdata);
		points_num = ARR2U16(pdata + 2); //chek for maximum 127!!

		uint16_t *reg_values;
		/*reg read callback*/
		err = RegReadCallback(start_addr, points_num, &reg_values);
		if (err)
		{
			/*Prepare exception*/
			resp_data = MBTCP_exception_pack(&mbap, fcode, err, &outlen);
		}
		else
		{
			uint16_t resp_bytes = points_num * 2; /*response bytes number*/
			resp_data = MBTCP_resp_pack(&mbap, fcode, resp_bytes + 1, &outlen);

			if (resp_data) {
				*(resp_data + 8) = (uint8_t) resp_bytes;
				for (int i = 0; i < points_num; i++)
				{
					U162ARR(reg_values[i], resp_data + 9 + 2*i);
				}
			}
		}
		break;

	/*Function 05: force single coil*/
	case MODBUS_FUNC_WRSCOIL:
		start_addr = ARR2U16(pdata);
		val = ARR2U16(pdata + 2);
		uint8_t c_val = 0;

		if (val)
		{
			if (val == 0xFF00) /*coil ON*/
				c_val = 1;
			else
			{
				resp_data = MBTCP_exception_pack(&mbap, fcode, MODBUS_ERR_ILLEGVAL, &outlen);
				break;
			}
		}

		err = CoilWriteCallback(start_addr, c_val);

		if (err)
		{
			/*Prepare exception*/
			resp_data = MBTCP_exception_pack(&mbap, fcode, err, &outlen);
		}
		else
		{
			resp_data = MBTCP_resp_pack(&mbap, fcode, 4, &outlen);
			if (resp_data)
			{
				U162ARR(start_addr, resp_data + 8);
				U162ARR(val, resp_data + 10);
			}
		}
		break;

	case MODBUS_FUNC_WRSREG: //write single register
		start_addr = ARR2U16(pdata);
		val = ARR2U16(pdata + 2);

			/*reg write callback*/
			MBerror err = RegWriteCallback(start_addr, val);
			if (err)
			{
				/*Prepare exception*/
				resp_data = MBTCP_exception_pack(&mbap, fcode, err, &outlen);
			}
			else
			{
				resp_data = MEMORY_MALLOC(inlen);
				if (resp_data)
				{
					outlen = inlen;
					memcpy(resp_data, indata, outlen);
				}
			}
		break;

	default:
		resp_data = MBTCP_exception_pack(&mbap, fcode, MODBUS_ERR_ILLEGFUNC, &outlen);
	}

	*outdata = resp_data;
	return outlen;
}
