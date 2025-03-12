/**
  ******************************************************************************
  * @file    module of modbus rtu
  * @author  arthur.qiang.li
  * @version V2
  * @date    22 feb 2020
  * @brief   V1 = mbtcp.c,v 1.3 2006/12/07 22:10:34 wolti Exp
  *  *
  ******************************************************************************
  */

/* ----------------------- System includes ----------------------------------*/
#include <stdint.h>
#include "stdlib.h"
#include "string.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbtcp.h"
#include "mbframe.h"
#include "mbconfig.h" //use cfg MB_TCP_ENABLED

#if MB_TCP_ENABLED > 0

/* ----------------------- Defines ------------------------------------------*/

/* ----------------------- MBAP Header --------------------------------------*/
/*
 *
 * <------------------------ MODBUS TCP/IP ADU ---------------------------->
 *                              <----------- MODBUS PDU ------------------->
 *  +-----------+---------------+------------------------------------------+
 *  | TID | PID | Length | UID  |Code | Data                               |
 *  +-----------+---------------+------------------------------------------+
 *  |     |     |        |      |                                           
 * (0)   (2)   (4)      (6)    (7)                                          
 *
 *   ... MB_TCP_TID          = 0 (Transaction Identifier - 2 Byte) 
 *   ... MB_TCP_PID          = 2 (Protocol Identifier - 2 Byte)
 *   ... MB_TCP_LEN          = 4 (Number of bytes - 2 Byte)
 *   ... MB_TCP_UID          = 6 (Unit Identifier - 1 Byte)
 *   ... MB_TCP_FUNC         = 7 (Modbus Function Code)
 *
 *   ... Modbus TCP/IP Application Data Unit
 *   ... Modbus Protocol Data Unit
 */

#define MB_TCP_TID          0
#define MB_TCP_PID          2
#define MB_TCP_LEN          4
#define MB_TCP_UID          6
#define MB_TCP_FUNC         7

#define MB_TCP_PROTOCOL_ID  0   /* 0 = Modbus Protocol fixed */


/*******************************************************************************
  * @brief  tcp init, it calls all of it's lowlevel port init functions and event init.
  *
  * @param  the slave     
  *
  * @retval 0=no error
  *
  * @note   22 Feb 2020 by liq.
  *****************************************************************************/
int32_t mb_tcp_init(MB_SLAVE_STRU *slave)
{
    int32_t  r;                                             //temp used return value.

    //ENTER_CRITICAL_SECTION();

                                                            //param check, make sure slave is not zero.
    if(slave == 0){
        return -1;
    }
                                                            //do event init first, for it will reset event, while following tcp init will post one.                                                        //init event
    if( slave->p_event_init() != 0) {
        return -2;
    }      
                                                            //do port init.
    r = slave->p_tcpsvr_init(slave->port_id);
    if( r != 0 ){ 
        return -3;
    }

    //EXIT_CRITICAL_SECTION(  );
    return 0;                                               //init successfully.
}


/*******************************************************************************
  * @brief  tcp enable or disable, by en/disable the tcp port.
  *
  * @param  slave = handle
            en = 1/0
  *
  * @retval none
  *
  * @note   [by liq, 22Feb2020]
  *****************************************************************************/
void mb_tcp_enable(MB_SLAVE_STRU *slave, int32_t en)
{
    /* param checking */
    if(slave == 0){
        return;
    }
    
    if(en == 0){                                            //to disable
        //ENTER_CRITICAL_SECTION(  );
        slave->p_tcpsvr_enable(0);                          //Make sure that no more clients are connected
        //EXIT_CRITICAL_SECTION(  );
    }
    else if(en == 1){                                       //to enable
        //ENTER_CRITICAL_SECTION(  );

        slave->p_tcpsvr_enable(1);

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
  * @note   23 Feb 2020
            what's doing?
            ...1.check pdu length and crc.
            
            when to call?
            ...in poll(), when get a event EV_FRAME_RECEIVED(meaning a completed frame), call this.

  *****************************************************************************/
int32_t mb_tcp_receive_pdu(MB_SLAVE_STRU *slave, uint8_t * oaddr, uint8_t ** opdu, uint16_t * opdulen)
{
    int               rtn_rcv;                              //rtn value of tcpsvr_receivng(), 0=ok
    uint16_t          mbap_pid;                             //temp fentch the PID in MBAP, this should be 0x0000 fixed

    rtn_rcv = slave->p_tcpsvr_receiving(slave->ucRTUBuf, &slave->pdu_len);
    if(rtn_rcv != 0){
        return rtn_rcv;
    }

                                                            //check if pid is 0x0000 correctly
    mbap_pid  = slave->ucRTUBuf[MB_TCP_PID] << 8U;
    mbap_pid |= slave->ucRTUBuf[MB_TCP_PID + 1];
    if(mbap_pid != MB_TCP_PROTOCOL_ID){
        return __LINE__;
    }
                                                            //now, pid check ok,
    *oaddr   = slave->address;                              //set it's addr or will not pass checking in poll()
    *opdu    = &(slave->ucRTUBuf[MB_TCP_FUNC]);             //pointer to RTUBuf[7]
    *opdulen = slave->pdu_len - MB_TCP_FUNC;

    return 0;

}


/*******************************************************************************
  * @brief  tcp send a frame, response or exception
  *
  * @param  addr = slave's address, useless in tcp
            pdu = pdu ptr, pointer to some where in slave->ucRTUBuf[]
            usLength = length of pdu
  *
  * @retval 0=OK
  *
  * @note   what's doing? 
            ...send the pdu, call port to asmber the adu.
            ...here use adu = pdu - 7, actuall, adu pointer is fixed slave->ucRTUBuf[]
  *****************************************************************************/
int32_t mb_tcp_send_pdu(MB_SLAVE_STRU *slave, uint8_t addr, const uint8_t * pdu, uint16_t pdulen )
{
    uint8_t   *p_adu;                                       //adu pointer
    uint16_t   adu_len;                                     //adu len


    p_adu   = (uint8_t *)pdu - MB_TCP_FUNC;                 //adu = pdu - 7
    if(slave->ucRTUBuf != p_adu){                           //check if padu is right, actuall padu is slave->ucRTUBuf[], there is no need to calculate
        return __LINE__;
    }
    adu_len = pdulen + MB_TCP_FUNC;                         //tcplen= pdulen + 7
    
                                                            /* The MBAP header is already initialized because we use slave->ucRTUBuf[] 
                                                            ...for both receive and send we only need to update the LEN of MBAP header,
                                                            ...it is speciall, LEN= pdulen+1, the '1' is the UID byte. */
    p_adu[MB_TCP_LEN]     = (pdulen + 1) >> 8U;             //d[4]
    p_adu[MB_TCP_LEN + 1] = (pdulen + 1) & 0xFF;            //d[5]
    
    slave->p_tcpsvr_send(p_adu, adu_len);

    return 0;
}


#endif //if MB_TCP_ENABLED > 0

