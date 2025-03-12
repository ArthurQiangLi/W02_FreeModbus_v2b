/**
  ******************************************************************************
  * @file    module of modbus ascii
  * @author  arthur.qiang.li
  * @version V2
  * @date    2020-0213
  * @brief   V1 = freemodbus/rtu/mbascii.c 2010/06/06 13:47:07 wolti Exp
  *  *
  ******************************************************************************
  */


/* ----------------------- System includes ----------------------------------*/
#include <stdint.h>
#include "stdlib.h"
#include "string.h"


/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbascii.h"
#include "mbframe.h"
#include "mbcrc.h" 
#include "mbconfig.h" //to see MB_ASCII_ENABLED


#if MB_ASCII_ENABLED > 0

/* ----------------------- Defines ------------------------------------------*/
#define MB_ASCII_DEFAULT_CR     '\r'                        // Default CR character for Modbus ASCII. */
#define MB_ASCII_DEFAULT_LF     '\n'                        // Default LF character for Modbus ASCII. */
#define MB_SER_PDU_SIZE_MIN     3                           // Minimum size of a Modbus ASCII frame. */
#define MB_SER_PDU_SIZE_MAX     256                         // Maximum size of a Modbus ASCII frame. */
#define MB_SER_PDU_SIZE_LRC     1                           // Size of LRC field in PDU. */
#define MB_SER_PDU_ADDR_OFF     0                           // Offset of slave address in Ser-PDU. */
#define MB_SER_PDU_PDU_OFF      1                           // Offset of Modbus-PDU in Ser-PDU. */

/* ----------------------- Type definitions ---------------------------------*/
typedef enum
{
    STATE_RX_IDLE,                                          // Receiver is in idle state. */
    STATE_RX_RCV,                                           // Frame is beeing received. */
    STATE_RX_WAIT_EOF                                       // Wait for End of Frame. */
} eMBRcvState;


typedef enum
{
    BYTE_HIGH_NIBBLE,                                       // Character for high nibble of byte. */
    BYTE_LOW_NIBBLE                                         // Character for low nibble of byte. */
} eMBBytePos;

/* ----------------------- Static functions ---------------------------------*/
static uint8_t    prvucMBCHAR2BIN( uint8_t c );

static uint8_t    prvucMBBIN2CHAR( uint8_t b );

static uint8_t    prvucMBLRC( uint8_t * p, uint16_t l );

static uint8_t    mb_ascii_parse_char(MB_SLAVE_STRU *slave,  uint16_t *p_rx_pos );

static int        mb_ascii_convert_to_ascii_frame(MB_SLAVE_STRU *slave, uint8_t d[], int n);

/*******************************************************************************
  * @brief  ascii init, it calls and init bsp serial and timer reload value.
  *
  * @param  slave     
  *
  * @retval 0=no error
  *
  * @note   The timer reload value is ONE second.
  *****************************************************************************/
int32_t mb_ascii_init(MB_SLAVE_STRU *slave)
{
    int32_t  r;

    //ENTER_CRITICAL_SECTION();
                                                            //* serial init, ascii use 7 bits, (while rtu use 8 bits). */
    r = slave->p_serial_init(slave->port_id, slave->baudrate, slave->databits, slave->parity);
    if( r != 0 ){
        return -1;
    }
    
    if( slave->p_timer_init(MB_ASCII_TIMEOUT_SEC * 20000) != 0 ) {
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
  * @brief  ascii enable or disable, by en/disable the serial and timer.
  *
  * @param  slave = handle;   en = 1/0
  *
  * @retval none
  *
  * @note   [by liq, feb 2020]
  *****************************************************************************/
void mb_ascii_enable(MB_SLAVE_STRU *slave, int32_t en)
{
                                                            //* param checking */
    if(slave == 0){
        return;
    }
    if(en == 0){                                            //disable
        //ENTER_CRITICAL_SECTION(  );
        slave->p_serial_enable( 0, 0 );             
        slave->p_timer_enable(0);
        //EXIT_CRITICAL_SECTION(  );
    }
    else if(en == 1){                                       //enable
        //ENTER_CRITICAL_SECTION(  );
        slave->p_serial_enable( 1, 0 );             
        //slave->p_timer_enable(1);
        slave->ascii_rcv_state = STATE_RX_IDLE;
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
            1.check pdu length and crc.
            the address may not be this slave, but this fucnction does not care, 
            the poll() will check address.
            when to call?
            when in poll(), get a event EV_FRAME_RECEIVED(meaning a completed frame), call this.
  *****************************************************************************/
int32_t mb_ascii_receive_pdu(MB_SLAVE_STRU *slave, uint8_t * oaddr, uint8_t ** opdu, uint16_t * opdulen)
{
    uint16_t readnum;                                       //the adu(addr, func-code, data, err-check) frame len
    //ENTER_CRITICAL_SECTION();

                                                            // receive data to array ucRTUBuf[]*/
    while(mb_ascii_parse_char(slave, &readnum) == 0){
        ;
    }

    
    if(readnum > MB_SER_PDU_SIZE_MAX){
        return -1;
    }

                                                            // Length and CRC check */
    if((readnum >= MB_SER_PDU_SIZE_MIN)
    && (prvucMBLRC((uint8_t *) slave->ucRTUBuf, readnum) == 0 ) )
    {
        *oaddr = slave->ucRTUBuf[MB_SER_PDU_ADDR_OFF];
                                                            //pdu size in rtu format, not ascii adu len
        *opdulen = ( uint16_t )(readnum - MB_SER_PDU_PDU_OFF - MB_SER_PDU_SIZE_LRC);

        *opdu = (uint8_t *) & (slave->ucRTUBuf[MB_SER_PDU_PDU_OFF]);
    }
    else {                                                  //length too short, in rtu. maybe serial io level error. */
        return -2;
    }

    //EXIT_CRITICAL_SECTION();
    return 0;
}


/*******************************************************************************
  * @brief  ascii start send a frame, response or exception
  *
  * @param  addr = slave's address
            pdu = pdu, point to slave.ucRTUBuf[1].
            pdulen = length of pdu
  *
  * @retval 0=OK
  *
  * @note   1. only start sending, do not know when sending is done.
            2. call port/ serial enable() and serial send().
            3. called by poll()
  *****************************************************************************/
int32_t mb_ascii_send_pdu(MB_SLAVE_STRU *slave, uint8_t addr, const uint8_t * pdu, uint16_t pdulen )
{
    uint8_t *     padu;                                     //ptr to slave.ucRTUBuf header
    uint16_t      adulen;                                   //len of adu
    uint16_t      lrc8;
                                                            /* Check if the receiver is still in idle state. If not we where too
                                                             * slow with processing the received frame and the master sent another
                                                             * frame on the network. We have to abort sending the frame.*/
    if( slave->ascii_rcv_state != STATE_RX_IDLE )
        return -1;

    //ENTER_CRITICAL_SECTION(  );

                                                            
    padu = (uint8_t *) pdu - 1;                             //adu = pdu - 1
    if(slave->ucRTUBuf != padu){                            //check if padu is right, actuall padu is slave->ucRTUBuf[], there is no need to calculate
        return __LINE__;
    }
    adulen = 1;
                                                            //adu[0] is addr
    padu[MB_SER_PDU_ADDR_OFF] = addr;
    adulen += pdulen;

                                                            //Calculate LRC checksum
    lrc8 = prvucMBLRC((uint8_t *) padu, adulen );
    slave->ucRTUBuf[adulen++] = lrc8;
    
    slave->p_serial_enable(0 /*RX*/, 1 /*TX*/); 

                                                            
    mb_ascii_convert_to_ascii_frame(slave, padu, adulen);   //convert array to ascii
    
    slave->p_serial_start_send(slave->p_ascii_txbuf, adulen*2 +3);

    //EXIT_CRITICAL_SECTION(  );
    return 0;
}



/*******************************************************************************
  * @brief  parse ONE received byte, when have a total frame, return 1.
  *
  * @param  none   
  * @retval 0=go on parse, 1=found a frame or data empty
  *
  * @note   A new character is received. If the character is a ':' the input
            buffer is cleared. A CR-character signals the end of the data block. 
            Other characters are part of the data block and their ASCII value 
            is converted back to a binary representation.
            when to call this?
            in offical v1.5 edition, this is 'pxMBFrameCBByteReceived' and call
            in rx byte isr when every byte received.
  *****************************************************************************/
static uint8_t mb_ascii_parse_char(MB_SLAVE_STRU *slave,  uint16_t *p_rx_pos )
{
    uint8_t    is_found = 0;                                //return value, if a completed frame is found
    uint8_t    ucByte;                                      //the char to be parsed
    uint8_t    ucResult;                                    //result of combine two char into a bin value.
    uint8_t    serial_num;                                  //0 or 1, return from p_serial_read_receive()

                                                            //read bsp ring buffer, get one byte
    serial_num = slave->p_serial_read_receive(&ucByte, 1);
    if(serial_num == 0){
        return 1;                                           //return, data empty.
    }
    
    switch ( slave->ascii_rcv_state )
    {
    case STATE_RX_RCV:
                                                            //* Enable timer for character timeout. */
        slave->p_timer_enable(1);
        if( ucByte == ':' ){                                //* Empty receive buffer. */
            slave->e_nibble = BYTE_HIGH_NIBBLE;
            slave->idx_rtubuf = 0;
        }
        else if( ucByte == MB_ASCII_DEFAULT_CR ){
            slave->ascii_rcv_state = STATE_RX_WAIT_EOF;
        }
        else{
            ucResult = prvucMBCHAR2BIN( ucByte );
            switch (slave->e_nibble)
            {
                                                            //* High nibble of the byte comes first. We check for a buffer overflow here. */
            case BYTE_HIGH_NIBBLE:
                if( slave->idx_rtubuf < MB_SER_PDU_SIZE_MAX ){
                    slave->ucRTUBuf[slave->idx_rtubuf] = ( uint8_t )( ucResult << 4 );
                    slave->e_nibble = BYTE_LOW_NIBBLE;
                    break;
                }
                else {                                      //*overflowed. not handled in Modbus specification but seems a resonable implementation. */
                    slave->ascii_rcv_state = STATE_RX_IDLE;
                                                            //* Disable previously activated timer because of error state. */
                    slave->p_timer_enable(0);
                }
                break;

            case BYTE_LOW_NIBBLE:
                slave->ucRTUBuf[slave->idx_rtubuf] |= ucResult;
                slave->idx_rtubuf++;
                slave->e_nibble = BYTE_HIGH_NIBBLE;
                break;
            }
        }
        break;

    case STATE_RX_WAIT_EOF:
        if( ucByte == MB_ASCII_DEFAULT_LF ){
                                                            //* Disable character timeout timer because all characters are received. */
            slave->p_timer_enable(0);
                                                            //* Receiver is again in idle state. */
            slave->ascii_rcv_state = STATE_RX_IDLE;
                                                            //* Notify the caller of eMBASCIIReceive that a new frame was received. */
            is_found = 1;                                   //xMBPortEventPost( EV_FRAME_RECEIVED );
        }
        else if( ucByte == ':' ) {
                                                            //* Empty receive buffer and back to receive state. */
            slave->idx_rtubuf = 0;
            slave->e_nibble = BYTE_HIGH_NIBBLE;
            slave->ascii_rcv_state = STATE_RX_RCV;
                                                            //* Enable timer for character timeout. */
            slave->p_timer_enable(1);
        }
        else {                                              //* Frame is not okay. Delete entire frame. */
            slave->ascii_rcv_state = STATE_RX_IDLE;
        }
        break;

    case STATE_RX_IDLE:
        if( ucByte == ':' ){
                                                            //* Enable timer for character timeout. */
            slave->p_timer_enable(1);
                                                            //* Reset the input buffers to store the frame. */
            slave->idx_rtubuf = 0;
            slave->e_nibble = BYTE_HIGH_NIBBLE;
            slave->ascii_rcv_state = STATE_RX_RCV;
        }
        break;
    }

    *p_rx_pos = slave->idx_rtubuf;
    return is_found;
}



/*******************************************************************************
  * @brief  convert tx array to tx ascii 
  *
  * @param  slave stru
  *
  * @retval error, 0=no err.
  *
  * @note   none
  *****************************************************************************/
static int mb_ascii_convert_to_ascii_frame(MB_SLAVE_STRU *slave, uint8_t d[], int n)
{
    int i, j;

    if(slave->p_ascii_txbuf == 0){
        return -1;                                          //buf not inited
    }
    if(n > MB_SER_PDU_SIZE_MAX){
        return -2;                                          //size not right
    }
                                                            //* Start of transmission. The start of a frame is defined by sending  the character ':'. */
    slave->p_ascii_txbuf[0] = ':';
                                                            /* Send the data block. Each data byte is encoded as a character hex stream,
                                                            ...the high nibble first,low nibble last. If all data bytes are exhausted 
                                                            ...we send a '\r' character to end the transmission. */
    for(i=0; i< n; i++){
        uint8_t db;                                         //data in bin
        uint8_t dc;                                         //data in char

        db = d[i] >> 4;
        dc = prvucMBBIN2CHAR(db);
        j = 2*i + 1;
        slave->p_ascii_txbuf[j] = dc;

        db = d[i] & 0x0F;
        dc = prvucMBBIN2CHAR(db);
        j = 2*i + 2;
        slave->p_ascii_txbuf[j] = dc;
    }
    j++;
    slave->p_ascii_txbuf[j] = MB_ASCII_DEFAULT_CR;
    j++;
    slave->p_ascii_txbuf[j] = MB_ASCII_DEFAULT_LF;
    //slave->ascii_txbuf_len = j+1;                           //store the len, for ascii_send_pdu() to use.

    return 0;
}

/*******************************************************************************
  * @brief  If we have a timeout we go back to the idle state and wait for the next frame.
  *
  * @param  none     
  * @retval none
  * @note   [2019.1017 by liq]function is reenterable ,called in isr.

    void TIM4_IRQHandler(void)
    {
      HAL_TIM_IRQHandler(&htim4);
      mb_rtu_t35_callback(slave1);      //like this
    }
    [by liq, 2019-11]
 ******************************************************************************/
void mb_ascii_t1s_callback(MB_SLAVE_STRU *slave)
{
    //slave->p_event_post( EV_FRAME_RECEIVED );

    slave->ascii_rcv_state = STATE_RX_IDLE;
    slave->p_timer_enable(0);

}

/*******************************************************************************
the below two ISR fucntion are call in uart isr. 
eg.

void USART3_IRQHandler(void)
{
  extern MB_SLAVE_STRU   mb_slave_rtu_p1;

  mb_rtu_bus_send_done_callback(&mb_slave_rtu_p1); //like this
  HAL_UART_IRQHandler(&huart3);
  mb_rtu_bus_idle_callback(&mb_slave_rtu_p1); //like this
}
[by liq, 2019-11]
 ******************************************************************************/
void mb_ascii_bus_idle_callback(MB_SLAVE_STRU *slave)
{
    if(slave->p_serial_check_IDLE()){
        //slave->p_timer_enable(1);
        slave->p_event_post( EV_FRAME_RECEIVED );
    }
}

void mb_ascii_bus_send_done_callback(MB_SLAVE_STRU *slave)
{
    if(slave->p_serial_check_TC()){
        slave->p_serial_enable( 1 /*rx*/, 0 );
    }
}



/*******************************************************************************
  * @brief  input '0'~'9' and 'A' to "F', convert to 0x0~0xf number
  *
  * @param  '0'~'9' and 'A' to "F'
  *
  * @retval the number, error return 0xff
  *
  * @note   none
  *****************************************************************************/
static uint8_t prvucMBCHAR2BIN( uint8_t ucCharacter )
{
    if( ( ucCharacter >= '0' ) && ( ucCharacter <= '9' ) ){
        return ( uint8_t )( ucCharacter - '0' );
    }
    else if( ( ucCharacter >= 'A' ) && ( ucCharacter <= 'F' ) ){
        return ( uint8_t )( ucCharacter - 'A' + 0x0A );
    }
    else{
        return 0xFF;
    }
}

static uint8_t prvucMBBIN2CHAR( uint8_t ucByte )
{
    if( ucByte <= 0x09 ){
        return ( uint8_t )( '0' + ucByte );
    }
    else if( ( ucByte >= 0x0A ) && ( ucByte <= 0x0F ) ){
        return ( uint8_t )( ucByte - 0x0A + 'A' );
    }
    else{
        /* Programming error. */
        return ' ';
    }
}


/*******************************************************************************
  * @brief  calculate a array's LRC
  *
  * @param  array pointer and its len.
  *
  * @retval LRC value
  *
  * @note   how to LRC? add all byte then get 2's complement (minus with 0)
  *****************************************************************************/
static uint8_t prvucMBLRC( uint8_t * pucFrame, uint16_t usLen )
{
    uint8_t           ucLRC = 0;                            //* LRC char initialized */

    while( usLen-- )
    {
        ucLRC += *pucFrame++;                               //* Add buffer byte without carry */
    }
                                                            //* Return twos complement */
    ucLRC = ( uint8_t ) ( -( ( int8_t ) ucLRC ) );
    return ucLRC;
}

#endif //#if MB_ASCII_ENABLED > 0

