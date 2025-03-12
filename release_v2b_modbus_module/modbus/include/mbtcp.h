
#ifndef _MB_TCP_H
#define _MB_TCP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mb.h"

/* Defines */
#define MB_TCP_PSEUDO_ADDRESS   255 //id, compatable with rtu's slave id
//add by liq 23Feb2020
#define MB_TCP_LEN              4
#define MB_TCP_UID              6
#define MB_TCP_FUNC             7
#define MB_TCP_BUF_SIZE         (256 + 7) /* Must hold a complete Modbus TCP frame. */

/* fucntion declaration */
int32_t mb_tcp_init(MB_SLAVE_STRU *slave);
void mb_tcp_enable(MB_SLAVE_STRU *slave, int32_t en);
int32_t mb_tcp_receive_pdu(MB_SLAVE_STRU *slave, uint8_t * pucRcvAddress, uint8_t ** pucFrame, uint16_t * pusLength);
int32_t mb_tcp_send_pdu(MB_SLAVE_STRU *slave, uint8_t ucSlaveAddress, const uint8_t * pucFrame, uint16_t pdulen );





#ifdef __cplusplus
}
#endif
#endif

