#include "simple_master.h"

/*MB Slaves*/
#define SLAVE_ADDR        1

#define RTLS_TRACE printf

/*ModBus*/
static mb_master_t MB_master;
static uint8_t mb_rx_buf[MODBUS_MAX_MSG_LEN];
static uint8_t mb_tx_buf[MODBUS_MAX_MSG_LEN];

osSemaphoreDef (rs485_tx_sem); // Declare semaphore RX485 Tx
osSemaphoreId  (rs485_tx_sem_id);  // Semaphore ID
osSemaphoreDef (rs485_rx_sem); // Declare semaphore RX485 Rx
osSemaphoreId  (rs485_rx_sem_id);  // Semaphore ID

MBerror RS485_Write(uint8_t *data, uint32_t len);
MBerror RS485_Read(uint32_t len);
MBerror ModBus_Wait4Response(uint32_t timeout);

int main(void)
{
    /*create semaphores for RS-485*/
    rs485_tx_sem_id = osSemaphoreCreate(osSemaphore(rs485_tx_sem), 1);
    rs485_rx_sem_id = osSemaphoreCreate(osSemaphore(rs485_rx_sem), 1);
    if (rs485_tx_sem_id == NULL && rs485_rx_sem_id == NULL)
    {
        RTLS_TRACE("RS-485 Semaphores alloc error\r\n");
        osThreadTerminate(NULL);;
    }
    
    /*Init ModBus Master*/
    MB_master.rx_buf = mb_rx_buf;
    MB_master.tx_buf = mb_tx_buf;
    MB_master.itfs_write = RS485_Write;
    MB_master.itfs_read = RS485_Read;
    MB_master.wait_for_resp = ModBus_Wait4Response;
    SiMasterInit(&MB_master);
    
    /*Init timer for RS485 DE delay*/
    TimerInit();
    
    
    
    return 1;
}

void ModbusPollThread(void const * argument)
{
    MBerror mb_err;
    
    uint8_t data_to_send[] = "Hello!";
    
    for (;;)
    {
        /*Write message to Tag*/
        mb_err = SiMasterWriteMRegs(&MB_master, SLAVE_ADDR, 1, sizeof(data_to_send), (uint16_t *) data_to_send);
        if (mb_err == MODBUS_ERR_OK)
        {

        }
        else
        {
        }
    }
}

MBerror RS485_Write(uint8_t *data, uint32_t len)
{
    MBerror err = MODBUS_ERR_OK;
    static uint32_t last_request_time = 0;
    
#if MB_SILENCE_PERIOD
    if (last_request_time + MB_SILENCE_PERIOD > osKernelSysTick())
    {
        osDelay(last_request_time + MB_SILENCE_PERIOD - osKernelSysTick());
    }
#endif
    
    osSemaphoreWait(rs485_tx_sem_id, 0);
    
    /*Assert DE pin*/
    RS485_DE_HIGH;
    
    TimerStart(2);
    
#if MB_TX_DMA
    if (len < 4)
    {
        if (HAL_UART_Transmit_IT(&huart8, data, len) != HAL_OK)
        {
            err = MODBUS_ERR_INTFS;
            //RTLS_TRACE_ERR("Modbus Tx Error\r\n");
        }
        else
        {
        }
    }
    else
    {
        /*Nonblocking write*/
        if (HAL_UART_Transmit_DMA(&huart8, data, len) != HAL_OK)
        {
            err = MODBUS_ERR_INTFS;
            //RTLS_TRACE_ERR("Modbus Tx Error\r\n");
        }
        else
        {
        }
    }
    
    /*Wait for transmitting*/
    if (osSemaphoreWait(rs485_tx_sem_id, RS485_TX_TIMEOUT) != osOK)
    {
        err = MODBUS_ERR_INTFS;
        //RTLS_TRACE_ERR("Modbus Tx Timeout\r\n");
    }
#else
    if (HAL_UART_Transmit(&huart8, data, len, RS485_TX_TIMEOUT) != HAL_OK)
    {
        err = MODBUS_ERR_INTFS;
        //RTLS_TRACE_ERR("Modbus Tx Error\r\n");
    }
#endif
    
    TimerStart(2);
    
    RS485_DE_LOW;
    
    last_request_time = osKernelSysTick();
    
    return err;
}

MBerror RS485_Read(uint32_t len)
{
    if (huart8.RxState != HAL_UART_STATE_READY)
    {
        HAL_UART_AbortReceive_IT(&huart8);
    }
    
    osSemaphoreWait(rs485_rx_sem_id, 0);
    
    RS485_RxChar = MB_master.rx_buf;
    MB_master.rx_wait_len = len;
    MB_master.rx_len = 0;
#if MB_RX_DMA
    /*Start receiving*/
    if (len < 4)
    {
        if (HAL_UART_Receive_IT(&huart8, MB_master.rx_buf, len) != HAL_OK)
        {
            //RTLS_TRACE_ERR("Modbus Rx Error\r\n");
            return MODBUS_ERR_INTFS;
        }
    }
    else
    {
        if (HAL_UART_Receive_DMA(&huart8, MB_master.rx_buf, len) != HAL_OK)
        {
            //RTLS_TRACE_ERR("Modbus Rx Error\r\n");
            return MODBUS_ERR_INTFS;
        }
    }
#else
    if (HAL_UART_Receive_IT(&huart8, RS485_RxChar, 1) != HAL_OK)
    {
        //RTLS_TRACE_ERR("Modbus Rx Error\r\n");
        return MODBUS_ERR_INTFS;
    }
#endif
    return MODBUS_ERR_OK;
}

MBerror ModBus_Wait4Response(uint32_t timeout)
{
    /*Wait for receiving*/
    if (osSemaphoreWait(rs485_rx_sem_id, timeout) != osOK)
    {
        //RTLS_TRACE_ERR("Modbus Rx Timeout\r\n");
        HAL_UART_AbortReceive_IT(&huart8);
        return MODBUS_ERR_INTFS;
    }
    
    return MODBUS_ERR_OK;
}

/*RS485 message sending callback*/
void RS485TxComplete_Callback(void)
{
    osSemaphoreRelease(rs485_tx_sem_id);
}

/*RS485 message receiving callback*/
void RS485RxComplete_Callback(void)
{
#if MB_RX_DMA
    MB_master.rx_len = MB_master.rx_wait_len;
    osSemaphoreRelease(rs485_rx_sem_id);
#else
    if ((MB_master.rx_len++) >= MB_master.rx_wait_len)
    {
        osSemaphoreRelease(rs485_rx_sem_id);
    }
    else
    {
        if (RS485_RxChar++ > &MB_master.rx_buf[sizeof(mb_rx_buf) - 1]) {
            RS485_RxChar = MB_master.rx_buf; }
        HAL_UART_Receive_IT(&huart8, RS485_RxChar, 1);
    }
#endif
}

/*Timer Initialization*/
void TimerInit(void)
{
    timer_sem_id = osSemaphoreCreate(osSemaphore(timer_sem), 1);
    osSemaphoreWait(timer_sem_id, 0);
}

/*Timer Start & wait*/
void TimerStart(uint32_t ui100ms_cnt)
{
    if (timer_sem_id != NULL)
    {
        timer_counter = 0;
        timer_timeout = ui100ms_cnt;
        HAL_TIM_Base_Start_IT(&htim2);
        
        if (osSemaphoreWait(timer_sem_id, osWaitForever) != osOK)
        {
            //error
        }
    }
}

/*Callback for timer*/
void TimerElapsedCallback(void)
{
    if (timer_sem_id != NULL)
    {
        if ((timer_counter++) >= timer_timeout)
        {
            HAL_TIM_Base_Stop_IT(&htim2);
            osSemaphoreRelease(timer_sem_id);
        }
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART8) //RS-485
    {
        RS485TxComplete_Callback();
    }
    
    osThreadYield();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart8) //RS-485
    {
        RS485RxComplete_Callback();
    }
    
    osThreadYield();
}
