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

#define MBAP_SIZE                   7                   /* MBAP header size */
#define EXCEPT_SIZE                 (MBAP_SIZE + 1 + 1) /*MBAP + Func code + Err*/

#ifndef MBTCP_SERVER_PORT
#define MBTCP_SERVER_PORT           502
#endif

#ifndef MODBUS_TCP_TASK_STACK
#define MODBUS_TCP_TASK_STACK       512
#endif

#ifndef MODBUS_TCP_TASK_PRIORITY
#define MODBUS_TCP_TASK_PRIORITY    (tskIDLE_PRIORITY + 3)
#endif

/* MBAP Header
 * Transaction ID: 2 bytes
 * Protocol ID: 2 bytes
 * Length: 2 bytes
 * Unit ID: 1 byte
 */
typedef struct {
    uint16_t tran_id; /*transaction ID*/
    uint16_t prot_id; /*protocol ID*/
    uint16_t plen; /*length*/
    uint8_t unit_id; /*Unit ID*/
} mbap_t;

static TaskHandle_t hMBTCP_Task = NULL;
#if MODBUS_REGS_ENABLE
extern MBerror RegInit(void *arg);
extern MBerror RegReadCallback(uint16_t addr, uint16_t num, uint16_t **regs);
extern MBerror RegsWriteCallback(uint16_t addr, uint16_t val);
#endif

static void MBTCP_Thread(void *arg);
static uint32_t MBTCP_PacketParser(MBTCP_Handle_t *mbtcp, uint32_t inlen);
static uint16_t MBTCP_Response(MBTCP_Handle_t *mbtcp, mbap_t *mbap_header, uint32_t resp_len);

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

    /* Create ModbusTCP thread */
    if (xTaskCreate(MBTCP_Thread,
                    "Modbus task",
                    MODBUS_TCP_TASK_STACK,
                    mbtcp,
                    MODBUS_TCP_TASK_PRIORITY,
                    &hMBTCP_Task) != pdPASS)
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
 * @param argument MBTCP Handle
 */
static void MBTCP_Thread(void *arg)
{
    MBTCP_Handle_t *mbtcp = (MBTCP_Handle_t*) arg;
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
        int r_sock = accept(sock, (struct sockaddr* ) &client_addr, &addr_len);

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

        while (1)
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
 * @brief               Response message composer
 * @param mbtcp         Pointer to MBTCP handler
 * @param mbap_header   Pointer to MBAP
 * @param resp_len      Response PDU data length
 * @return              Response packet length
 */
static uint16_t MBTCP_Response(MBTCP_Handle_t *mbtcp, mbap_t *mbap_header, uint32_t resp_len)
{
    MB_ASSERT(mbtcp->tx_buf_size >= MBAP_SIZE + resp_len);

    uint8_t *outdata = mbtcp->tx_buf;

    mbap_header->plen = resp_len + 1; /* PDU len + Unit ID */

    U162ARR(mbap_header->tran_id, outdata);
    U162ARR(mbap_header->prot_id, &outdata[2]);
    U162ARR(mbap_header->plen, &outdata[4]);
    outdata[6] = mbap_header->unit_id;

    return MBAP_SIZE + resp_len;
}

/**
 * @brief       Incoming packet parser
 * @param mbtcp Pointer to MBTCP handler
 * @param inlen Packet length
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
    mbap.tran_id = ARR2U16(indata);     /*transaction ID*/
    mbap.prot_id = ARR2U16(&indata[2]); /*protocol ID*/
    mbap.plen = ARR2U16(&indata[4]);    /*length*/
    mbap.unit_id = indata[6];           /*Unit ID*/

    if (mbap.prot_id != 0)
    {
        MODBUS_TRACE("Incorrect Protocol ID: %d\r\n", mbap.prot_id);
        return 0;
    }

    if ((mbap.unit_id != mbtcp->unit) || (mbap.unit_id > 247))
    {
        MODBUS_TRACE("Incorrect unit ID: %d\r\n", mbap.unit_id);
        return 0;
    }

    /*--PDU---*/
    uint8_t *pPDU = &indata[MBAP_SIZE];
    uint8_t *pResp = &mbtcp->tx_buf[MBAP_SIZE];

    err = MB_PDU_Parser(pPDU, pResp, &resp_len);

    if (resp_len > 0)
    {
        if (err != MODBUS_ERR_OK)
        {
            MODBUS_TRACE("Function error: %d\r\n", err);
        }

        /* Response prepare */
        outlen = MBTCP_Response(mbtcp, &mbap, resp_len);
    }

    return outlen;
}
