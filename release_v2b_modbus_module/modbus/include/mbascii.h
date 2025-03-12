/**
  ******************************************************************************
  * @file    HEADER FILE, public functions of mb ascii transport layer
  * @author  arthur.qiang.li
  * @version V2
  * @date    2020-2-3
  * @brief   V1 mbascii.h,v 1.8 2006/12/07 22:10:34 wolti Exp
             V2 2020-2-3 modify multi-instance
  *                      
  ******************************************************************************
  */


/* 

 * File: $Id: mbascii.h,v 1.8 2006/12/07 22:10:34 wolti Exp $
 */

#ifndef _MB_ASCII_H
#define _MB_ASCII_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mbconfig.h"

#if MB_ASCII_ENABLED > 0

extern int32_t mb_ascii_init(MB_SLAVE_STRU *slave);
extern void    mb_ascii_enable(MB_SLAVE_STRU *slave, int32_t en);
extern int32_t mb_ascii_receive_pdu(MB_SLAVE_STRU *slave, uint8_t * pucRcvAddress, uint8_t ** pucFrame, uint16_t * pusLength);
extern int32_t mb_ascii_send_pdu(MB_SLAVE_STRU *slave, uint8_t ucSlaveAddress, const uint8_t * pucFrame, uint16_t usLength );

extern void    mb_ascii_t1s_callback(MB_SLAVE_STRU *slave);
extern void    mb_ascii_bus_idle_callback(MB_SLAVE_STRU *slave);
extern void    mb_ascii_bus_send_done_callback(MB_SLAVE_STRU *slave);

    
    
#endif

#ifdef __cplusplus
}
#endif
#endif

