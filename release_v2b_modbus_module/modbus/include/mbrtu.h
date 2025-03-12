

#ifndef _MB_RTU_H
#define _MB_RTU_H

#ifdef __cplusplus
extern "C" {
#endif


#include "mb.h"

/* fucntion declaration */
int32_t mb_rtu_init(MB_SLAVE_STRU *slave);
void    mb_rtu_enable(MB_SLAVE_STRU *slave, int32_t en);
int32_t mb_rtu_receive_pdu(MB_SLAVE_STRU *slave, uint8_t * pucRcvAddress, uint8_t ** pucFrame, uint16_t * pusLength);
int32_t mb_rtu_send_pdu(MB_SLAVE_STRU *slave, uint8_t ucSlaveAddress, const uint8_t * pucFrame, uint16_t usLength );

void    mb_rtu_t35_callback(MB_SLAVE_STRU *slave);
void    mb_rtu_bus_idle_callback(MB_SLAVE_STRU *slave);
void    mb_rtu_bus_send_done_callback(MB_SLAVE_STRU *slave);


#ifdef __cplusplus
}
#endif
#endif

