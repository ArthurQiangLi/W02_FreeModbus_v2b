/**
  ******************************************************************************
  * @file    module of modbus rtu
  * @author  arthur.qiang.li
  * @version V2
  * @date    2019-1016
  * @brief   V1 = freemodbus/rtu/mbrtu.c
  *  *
  ******************************************************************************
  */

/* ----------------------- System includes ----------------------------------*/
#include <stdint.h>
#include "stdlib.h"
#include "string.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbrtu.h"
#include "mbframe.h"
#include "mbcrc.h"


/*******************************************************************************
******************************** Private define ********************************
*******************************************************************************/
#define MB_SER_PDU_SIZE_MIN     4       /*!< Minimum size of a Modbus RTU frame. */
#define MB_SER_PDU_SIZE_MAX     256     /*!< Maximum size of a Modbus RTU frame. */
#define MB_SER_PDU_SIZE_CRC     2       /*!< Size of CRC field in PDU. */
#define MB_SER_PDU_ADDR_OFF     0       /*!< Offset of slave address in Ser-PDU. */
#define MB_SER_PDU_PDU_OFF      1       /*!< Offset of Modbus-PDU in Ser-PDU. */


/*******************************************************************************
  * @brief  rtu init, it calls and init bsp serial and timer
  *
  * @param  slave     
  *
  * @retval 0=no error
  *
  * @note   
             The timer reload value for a character is given by:
              ChTimeValue = Ticks_per_1s / ( Baudrate / 11 )
                          = 11 * Ticks_per_1s / Baudrate
                          = 220000 / Baudrate
              The reload for t3.5 is 1.5 times this value and similary
              for t3.5.
              If baudrate > 19200 then we should use the fixed timer values,
              t35 = 1750us. Otherwise t35 must be 3.5 times the character time.
  *****************************************************************************/
int32_t mb_rtu_init(MB_SLAVE_STRU *slave)
{
    uint32_t n_50us;                                        //for timer reload value, how many 50 us we need
    int32_t  r;

    //ENTER_CRITICAL_SECTION();

                                                            //* serial init for rtu, rtu use 8 databits fixly . */
    r = slave->p_serial_init(slave->port_id, slave->baudrate, slave->databits, slave->parity);
    if( r != 0 ){
        return -1;
    }
    
    if( slave->baudrate > 19200 ){
        n_50us = 35;                                        //* 1,750us. */
    }
    else{
        n_50us = ( 7UL * 220000UL ) / ( 2UL * (slave->baudrate) );
    }
                                                            //set timer's period
    if( slave->p_timer_init(( uint16_t ) n_50us) != 0 ) {
        return -2;
    }
                                                            //init event
    if( slave->p_event_init() != 0) {
        return -3;
    }
    //EXIT_CRITICAL_SECTION(  );

    return 0;                                               //init successfully
}

/*******************************************************************************
  * @brief  rtu enable or disable, by en/disable the serial and timer.
  *
  * @param  slave = handle
            en = 1/0
  *
  * @retval none
  *
  * @note   [by liq, 2019-11]
  *****************************************************************************/
void mb_rtu_enable(MB_SLAVE_STRU *slave, int32_t en)
{
    /* param checking */
    if(slave == 0){
        return;
    }
    
    if(en == 0){
        //ENTER_CRITICAL_SECTION(  );
        slave->p_serial_enable( 0, 0 );             
        slave->p_timer_enable(0);
        //EXIT_CRITICAL_SECTION(  );
    }
    else if(en == 1){
        //ENTER_CRITICAL_SECTION(  );
        slave->p_serial_enable( 1, 0 );             
        slave->p_timer_enable(1);
        //EXIT_CRITICAL_SECTION( );

    }
    else{
        return;
    }
}


/*******************************************************************************
  * @brief  check received string, and get the addr, pdu, len of pdu,
            after some bytes are received.
  *
  * @param  oaddr = output the address
            opdu = output the pdu array, a pointer, should be ucRTUBuf[7]
            opdulen = output length of pdu.
  *
  * @retval 0 = OK, EIO= length too short.
  *
  * @note   [2019.1016]
            what's doing?
            ...1.check pdu length and crc, 2.call by the poll(), after a t35 timer isr.
            ...the address may not be this slave, but this fucnction does not care, the poll() will check address.
            ...[note] you should know that, this layer operate data in ucRTUBuf[], it's a copy of port layer.
            when to call?
            ...in poll(), when get a event EV_FRAME_RECEIVED(meaning a completed frame), call this.
  *****************************************************************************/
int32_t mb_rtu_receive_pdu(MB_SLAVE_STRU *slave, uint8_t * oaddr, uint8_t ** opdu, uint16_t * opdulen)
{
    uint16_t readnum;                                       //is how many bytes actually read from serial port.
    uint16_t crc_err;

    //ENTER_CRITICAL_SECTION();
                                                            // receive data to array ucRTUBuf[]*/
    readnum = slave->p_serial_read_receive((uint8_t *)(slave->ucRTUBuf), MB_SER_PDU_SIZE_MAX);
    if(readnum > MB_SER_PDU_SIZE_MAX){
        return -1;
    }

                                                            //length and CRC check */
    if(readnum < MB_SER_PDU_SIZE_MIN){ //readnum >= MB_SER_PDU_SIZE_MIN
        return -2;//* length too short, in rtu. maybe serial io level error. */
    }
    
    crc_err = usMBCRC16((uint8_t *)(slave->ucRTUBuf), readnum);
    
    if(crc_err != 0){
        return -3;
    }
    
    *oaddr = slave->ucRTUBuf[MB_SER_PDU_ADDR_OFF];

    *opdulen = (uint16_t)(readnum - MB_SER_PDU_PDU_OFF - MB_SER_PDU_SIZE_CRC);

    *opdu = (uint8_t *) & (slave->ucRTUBuf[MB_SER_PDU_PDU_OFF]);


    //EXIT_CRITICAL_SECTION();
    return 0;                                               //receive a pdu with no err.
}



/*******************************************************************************
  * @brief  rtu start send a frame, response or exception
  *
  * @param  addr = slave's address
            pdu = pdu pointer
            pdulen = length of pdu
  *
  * @retval 0=OK
  *
  * @note   what's doing? 
  			...1. only start sending, do not know when sending is done.
            ...2. call port/ serial enable() and serial send().
            ...adu: [ addr(1B) |  pdu(function code 1B + data NB)  | CRC(2B) ]
                    -----------  ---------------------------------   --------
  *****************************************************************************/
int32_t mb_rtu_send_pdu(MB_SLAVE_STRU *slave, uint8_t addr, const uint8_t * pdu, uint16_t pdulen )
{
    uint8_t *     padu;                                     //pointer to adu, that is one byte befor pdu, it is also ucRTUBuf[0] fixed
    uint16_t      adulen;                                   //len of adu
    uint16_t      val_crc16;                                //value of crc16

    //ENTER_CRITICAL_SECTION(  );

    padu = ( uint8_t * ) pdu - 1;                           //adu = pdu - 1
    if(slave->ucRTUBuf != padu){                            //check if padu is right, actuall padu is slave->ucRTUBuf[], there is no need to calculate
        return __LINE__;
    }
    adulen = 1;
                                                            
    padu[MB_SER_PDU_ADDR_OFF] = addr;                       //adu[0] is addr
    adulen += pdulen;

                                                            //Calculate CRC16 checksum
    val_crc16 = usMBCRC16( ( uint8_t * ) padu, adulen );
    slave->ucRTUBuf[adulen++] = ( uint8_t )( val_crc16 & 0xFF);
    slave->ucRTUBuf[adulen++] = ( uint8_t )( val_crc16 >> 8);
    
    slave->p_serial_enable(0 , 1 /*TX*/ ); 
    slave->p_serial_start_send((uint8_t*)padu, adulen);     //adulen = 1 + pdulen + 2

    //EXIT_CRITICAL_SECTION(  );
    return 0;
}


/*******************************************************************************
  * @brief  Notify the listener that a new frame was received.
  *
  * @param  slave     
  * @retval none
  *
  * @note   [2019.1017 by liq]
            this function is reenterable , it's called in isr.

void TIM4_IRQHandler(void)
{
  extern MB_SLAVE_STRU   mb_slave_rtu_01;

  HAL_TIM_IRQHandler(&htim4);
  mb_rtu_t35_callback(&mb_slave_rtu_01); //like this
}
[by liq, 2019-11]
 ******************************************************************************/
void mb_rtu_t35_callback(MB_SLAVE_STRU *slave)
{

    slave->p_event_post( EV_FRAME_RECEIVED );
    
    slave->p_timer_enable(0);

}

/*******************************************************************************
the below two ISR fucntion are call in uart isr. 
eg.

void USART3_IRQHandler(void)
{
  extern MB_SLAVE_STRU   mb_slave_rtu_01;

  mb_rtu_bus_send_done_callback(&mb_slave_rtu_01); //like this
  HAL_UART_IRQHandler(&huart3);
  mb_rtu_bus_idle_callback(&mb_slave_rtu_01); //like this
}
[by liq, 2019-11]
 ******************************************************************************/
void mb_rtu_bus_idle_callback(MB_SLAVE_STRU *slave)
{
    if(slave->p_serial_check_IDLE()){
        slave->p_timer_enable(1);
    }
}

void mb_rtu_bus_send_done_callback(MB_SLAVE_STRU *slave)
{
    if(slave->p_serial_check_TC()){
        slave->p_serial_enable( 1 /*rx*/, 0 );
    }
}


/********************************* end of file ********************************/

