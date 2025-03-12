/**
  ******************************************************************************
  * @file    mb_port_rtu.c for a specific serial rtu port
  * @author  arthur.qiang.li
  * @version V1 
  * @date    2018-1-12
  * @brief   2020-2-11 turn three port.c into one .c file.
  *
  ******************************************************************************
  */

/*******************************************************************************
*******************************   cfg and const    *****************************
*******************************************************************************/

                                                            /* if you will use freertos's event queue in modbus rtu event message 
                                                            0= dont use, and you must set a osDelay() in mb_poll(),
                                                            1= use queue, and you do not need to set a osdelay() in mb_poll(), */
#define MB_PORT_EVENT_USE_QUEUE                          (1)

                                                            /* if use bsp_tp module for testing and measuring */
#define MB_PORT_USE_BSP_TP                               (1)

#define MB_PORT_ADDRESS                                  (3)


#define MB_PORT_SERIAL_NAME                         (huart2)  

#define MB_PORT_SERIAL_BAUD                         (115200)
                                                            //tx dma (also for ringbuf) size, but rtu do not use ringbuf to send, it send directly, so set it to 1.
#define MB_PORT_UART_TXDMA_SIZE                          (1)                      
                                                            //rx dma (also for ringbuf) size, a ascii adu is 513 bytes max, we make it double.
#define MB_PORT_UART_RXDMA_SIZE                       (1000)                     
                                                            /* tim will use timer's output comapare irq.
                                                            hardware setting, we use a CC channel in a timer, 
                                                            each rtu instance occupy a whole timer */
#define MB_PORT_TIM_HANDLER                           htim11
#define MB_PORT_TIM_CHANNEL                    TIM_CHANNEL_1
#define MB_PORT_TIM_IT_FLAG                       TIM_IT_CC1


/*******************************************************************************
************************************ Includes **********************************
*******************************************************************************/

//---call some lib---
#if (MB_PORT_EVENT_USE_QUEUE == 1)
    #include "cmsis_os.h"
#endif

//---call some lowlevel---
#include "tim.h"      //call timer perpherial
#include "bsp_serial.h" // contains "usart.h"

//---call some task/module---
#include "mb.h"      //data type of slave structure
#include "bsp_tp.h"  // for test only. 2019.1013 by liq.

/*******************************************************************************
********************************* Private macro ********************************
*******************************************************************************/
#if (MB_PORT_USE_BSP_TP == 1)
    #define TP_HI(pin)      bsp_tp_high(pin)
    #define TP_LO(pin)      bsp_tp_low(pin)
#else
    #define TP_HI(pin)      
    #define TP_LO(pin)      
#endif

/*******************************************************************************
******************************** Private typedef *******************************
*******************************************************************************/
                                                            // private data of this module
typedef struct
{
#if (MB_PORT_EVENT_USE_QUEUE == 1)
    xQueueHandle    event_q_handler;                        //os queue for port to send msg to mb lib.
#else
    uint32_t        event_value;                            //the value of the message
    int32_t         event_is_valid;                         //0=invalid, 1=valid, whether there is a valid evernt in above 'store'
#endif

                                                            //in us, store the calculated CNT register value for a timer
                                                            //each time when timer enable, CC= CNT+this.
    uint32_t        tim_cnt_us; 


    BSP_SERIAL_STRU serial_handler;                         //serial handler, eg. huart2.
    uint8_t         serial_txdmabuf [MB_PORT_UART_TXDMA_SIZE];//buf for lowlevel bsp serial tx dma.
    uint8_t         serial_rxdmabuf [MB_PORT_UART_RXDMA_SIZE];//buf for lowlevel bsp serial rx dma.
    uint8_t         ascii_adu[513];
    
} MBPORT_ASCII_STRU;

/*******************************************************************************
******************************* Private variables ******************************
*******************************************************************************/
                                                            /*private data of this module,
                                                            ...data rtu is not connected to the slave stru, the slave stru connects to
                                                            ...the functions in this file, while the functions connect data rtu.*/
static MBPORT_ASCII_STRU      rtu;

/*******************************************************************************
******************************* event for port    ******************************
*******************************************************************************/


/*******************************************************************************
  * @brief  event init
  *
  * @param  None
  *
  * @retval 0= no error
  *****************************************************************************/
static int32_t event_init(void)
{
#if (MB_PORT_EVENT_USE_QUEUE == 1)

    rtu.event_q_handler = xQueueCreate( 1, sizeof( uint32_t ) );
    if(rtu.event_q_handler != NULL){
        return 0;
    }else{
        return -1;
    }
    
#else
    rtu.event_is_valid = 0;
    return 0;
#endif
}


/*******************************************************************************
  * @brief  send event
  *
  * @param  eEvent
  *
  * @retval 0= no error
  *****************************************************************************/
static int32_t event_post(uint32_t e)
{
#if (MB_PORT_EVENT_USE_QUEUE == 1)
    portBASE_TYPE   xEventSent = pdFALSE;

    xEventSent = xQueueSendFromISR( rtu.event_q_handler, &e, &xEventSent );
    
    if(xEventSent == pdTRUE)
        return 0;
    else 
        return -1;
#else
    rtu.event_is_valid  = 1;  
    rtu.event_value     = e;  
    
    return 0;
#endif
}   


/*******************************************************************************
  * @brief  get event
  *
  * @param  *e
  *
  * @retval 0= no error
  * 
  * @note xBlockTime The time in ticks to wait for the semaphore to become
  * available.  The macro portTICK_PERIOD_MS can be used to convert this to a
  * real time.  A block time of zero can be used to poll the semaphore.  A block
  * time of portMAX_DELAY can be used to block indefinitely (provided
  * INCLUDE_vTaskSuspend is set to 1 in FreeRTOSConfig.h).
  *****************************************************************************/
static int32_t event_get(uint32_t * e)
{
#if (MB_PORT_EVENT_USE_QUEUE == 1)
    uint32_t waittime = portMAX_DELAY;
    if(xQueueReceive(rtu.event_q_handler, e, waittime) == pdTRUE){
        return 0;
    }
    return -1;
    
#else
    if(rtu.event_is_valid){
        *e = rtu.event_value;
        rtu.event_is_valid = 0;
        return 0;
    }
    return -1;
    
#endif
}

/*******************************************************************************
********************************  tim for port   *******************************
*******************************************************************************/


/*******************************************************************************
  * @brief  timer init
  *
  * @param  usTim1Timerout50us
  *
  * @retval int8_t
  *****************************************************************************/
static int32_t timer_init(uint32_t n_50us)
{
    TIM_OC_InitTypeDef sConfigOC;

    rtu.tim_cnt_us = n_50us;

    sConfigOC.OCMode = TIM_OCMODE_TIMING;
    sConfigOC.Pulse = rtu.tim_cnt_us; 
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_OC_ConfigChannel(&MB_PORT_TIM_HANDLER, &sConfigOC, MB_PORT_TIM_CHANNEL) != HAL_OK){
        return -1;
    }

    HAL_TIM_Base_Start(&MB_PORT_TIM_HANDLER);
    
    return 0;
}

/*******************************************************************************
  * @brief  Enable the timer with the timeout passed to timer_init( )
  *
  * @param  en = 0/1
  *
  * @retval none
  *****************************************************************************/
static void timer_enable(uint32_t en)
{
    if(en == 1){
        uint32_t tnow, tthen;
        
        tnow  = __HAL_TIM_GET_COUNTER(&MB_PORT_TIM_HANDLER);
        tthen = tnow + (rtu.tim_cnt_us);                    //take advantage of unsigned data's overflow recrusive feature.
        
        __HAL_TIM_SET_COMPARE(&MB_PORT_TIM_HANDLER,MB_PORT_TIM_CHANNEL, tthen);
        __HAL_TIM_CLEAR_IT(&MB_PORT_TIM_HANDLER, MB_PORT_TIM_IT_FLAG);
        __HAL_TIM_ENABLE_IT(&MB_PORT_TIM_HANDLER, MB_PORT_TIM_IT_FLAG);
        
        TP_HI(2);
    }
    else if(en == 0){

        __HAL_TIM_DISABLE_IT(&MB_PORT_TIM_HANDLER, MB_PORT_TIM_IT_FLAG);
        __HAL_TIM_CLEAR_IT(&MB_PORT_TIM_HANDLER, MB_PORT_TIM_IT_FLAG);
        TP_LO(2);
    }
    else{
        return;
    }
}

/*******************************************************************************
******************************** serial for port *******************************
*******************************************************************************/


/*******************************************************************************
  * @brief  config serail, low level initialization is done in uart.c
  *
  * @param  ucPORT      
  * @param  ulBaudRate  
  * @param  ucDataBits  
  * @param  eParity     
  *
  * @retval 0= no error.
  *****************************************************************************/
static int32_t serial_init( uint8_t port, uint32_t baudrate, uint8_t databits, uint8_t parity)
{
    (void)port;                                             
    (void)databits;                                         
    (void)parity;                                            

    rtu.serial_handler.huart        = &MB_PORT_SERIAL_NAME;
    rtu.serial_handler.baud         = baudrate;
    rtu.serial_handler.ptxbuf       = rtu.serial_txdmabuf;
    rtu.serial_handler.prxbuf       = rtu.serial_rxdmabuf;
    rtu.serial_handler.txbufsize    = MB_PORT_UART_TXDMA_SIZE;
    rtu.serial_handler.rxbufsize    = MB_PORT_UART_RXDMA_SIZE;
    rtu.serial_handler.if_has_rxdma = 1;
    bsp_serial_init(&(rtu.serial_handler));                 //connect to hw and initied.

                                                            //* send a testing string. */
                                                            //bsp_serial_buf_puts(&mb_port2, "hello, doggy 1!\n");
    bsp_serial_flush_txbuf(&(rtu.serial_handler));

    return 0;
}


/*******************************************************************************
  * @brief  serial bus io control
  *
  * @param  xRxEnable = ON/OFF
  * @param  xTxEnable
  *
  * @retval 2019.1013 by liq.
  *****************************************************************************/
static void serial_enable(uint32_t enrx, uint32_t entx)
{
                                                            //*-1- for RX */
    if(enrx){
                                                            //* start rxing */
        bsp_serial_reset_rxbuf_index(&(rtu.serial_handler));            
        
                                                            //* enable idle irq */
        __HAL_UART_CLEAR_IDLEFLAG(rtu.serial_handler.huart);
        bsp_serial_enable_idle_irq( &(rtu.serial_handler), 1);
        
                                                            //* makesure enable rx */
        bsp_serial_rx_enable(&(rtu.serial_handler), 1);     //*enable*/
        TP_HI(4);
    }else {
                                                            //bsp_serial_rx_enable(&mb_port2, 0); /*disable, [CAUTION!] reset and set will trigger IDLE isr even if no data received. liq 2019.1013. */
        
                                                            //* disable idle irq */
        __HAL_UART_CLEAR_IDLEFLAG(rtu.serial_handler.huart);
        bsp_serial_enable_idle_irq( &(rtu.serial_handler), 0);
        TP_LO(4);
    }

    
                                                            //*-2- for TX */
    if(entx){                                               //*enable*/
        bsp_serial_tx_enable(&(rtu.serial_handler), 1);                 
        TP_HI(3);
    }
    else {                                                  //*disable*/
        bsp_serial_tx_enable(&(rtu.serial_handler), 0);                 
        TP_LO(3);
    }    
    
}


/*******************************************************************************
  * @brief  fill data to txbuf and flush, but DO NOT wait for the end.
  *
  * @param  d = data, n = num of data
  *
  * @retval none
  * @notte  none
  *****************************************************************************/
static void serial_start_send(uint8_t d[], int n)
{
    bsp_serial_buf_put_array_directly(&(rtu.serial_handler), d, n);
}


/*******************************************************************************
  * @brief  fentch rx ringbuf data and return the number.
  *
  * @param  d= where to store the rx data
            n= how many you want to read
  *
  * @retval how many you actually got.
  * @notte  none
  *****************************************************************************/
static int32_t serial_read_receive(uint8_t d[], int n)
{
                                                            //copy data from bsp_serial to d[]
    return bsp_serial_buf_read_array(&(rtu.serial_handler), d, n);
}


/*******************************************************************************
  * @brief  char TC isr handler
  *
  * @param  none     
  *
  * @retval none
  *
  * @note   [2019.1017 by liq]
            this function is called in isr, no need to modify it.
  *****************************************************************************/
static int32_t serail_check_TC(void)
{
    int is_flag;
    int is_it;
    
    is_flag = __HAL_UART_GET_FLAG(rtu.serial_handler.huart, UART_FLAG_TC);
    is_it   = __HAL_UART_GET_IT_SOURCE(rtu.serial_handler.huart ,UART_IT_TC);
    
    if(is_flag && is_it)
    {
        TP_HI(1);
        TP_LO(1);        
        return 1;
    }
    return 0;
}


/*******************************************************************************
  * @brief  IDLE isr handler, clear idle flag only
  *
  * @param  none
  *
  * @retval none
  * @notte  how to clear idle flag, refer to stm32fxxx_hal_uart.h
            for f1, read SR then read DR will clear IDLE
            for f3, It is cleared by software, writing 1 to the IDLECF in the USARTx_ICR register.
  *****************************************************************************/
static int32_t serial_check_IDLE(void)
{
    int is_flag;
    int is_it;
    
    is_flag = __HAL_UART_GET_FLAG(rtu.serial_handler.huart, UART_FLAG_IDLE);
    is_it   = __HAL_UART_GET_IT_SOURCE(rtu.serial_handler.huart ,UART_IT_IDLE);
    
    if(is_flag && is_it){                                   //clear idle flag.
        TP_HI(0);
        __HAL_UART_CLEAR_IDLEFLAG(rtu.serial_handler.huart);          
        TP_LO(0);        
        return 1;
    }
    return 0;
}



                                                            //public data of this module
MB_SLAVE_STRU  mb_slave_ascii_03=
{
                                                            /* cfg all the handler of the slave instance*/
    .address               = MB_PORT_ADDRESS,
    .mode                  = MB_ASCII,
    .baudrate              = MB_PORT_SERIAL_BAUD,
    .p_event_init          = event_init,
    .p_event_post          = event_post,
    .p_event_get           = event_get,
    .p_serial_init         = serial_init,
    .p_serial_enable       = serial_enable,
    .p_serial_start_send   = serial_start_send,
    .p_serial_read_receive = serial_read_receive,
    .p_serial_check_TC     = serail_check_TC,
    .p_serial_check_IDLE   = serial_check_IDLE,
    .p_timer_init          = timer_init,
    .p_timer_enable        = timer_enable,
    .p_ascii_txbuf         = rtu.ascii_adu,
};


/********************************* end of file ********************************/

