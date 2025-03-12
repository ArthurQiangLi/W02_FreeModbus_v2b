/* 
 * This module defines the interface for the application. It contains
 * the basic functions and types required to use the Modbus protocol stack.
 * A typical application will want to call eMBInit() first. If the device
 * is ready to answer network requests it must then call eMBEnable() to activate
 * the protocol stack. In the main loop the function eMBPoll() must be called
 * periodically. The time interval between pooling depends on the configured
 * Modbus timeout. If an RTOS is available a separate task should be created
 * and the task should always call the function eMBPoll().
 *
 * \code
 * // Initialize protocol stack in RTU mode for a slave with address 10 = 0x0A
 * eMBInit( MB_RTU, 0x0A, 38400, MB_PAR_EVEN );
 * // Enable the Modbus Protocol Stack.
 * eMBEnable(  );
 * for( ;; )
 * {
 *     // Call the main polling loop of the Modbus protocol stack.
 *     eMBPoll(  );
 *     ...
 * }

  * Modbus serial supports two transmission modes. Either ASCII or RTU. RTU
 * is faster but has more hardware requirements and requires a network with
 * a low jitter. ASCII is slower and more reliable on slower links (E.g. modems)

 * File: $Id: mb.h,v 1.17 2006/12/07 22:10:34 wolti Exp $
 */

#ifndef _MB_H
#define _MB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------------- Defines ------------------------------------------*/

/*brief Use the default Modbus TCP port (502) */
#define MB_TCP_PORT_USE_DEFAULT         ( 0 )   



/* ----------------------- Type definitions ---------------------------------*/

/* Modbus serial transmission modes, 
not used, [by liq, 2019-11] */
typedef enum
{
    MB_RTU,                     /*!< RTU transmission mode. */
    MB_ASCII,                   /*!< ASCII transmission mode. */
    MB_TCP                      /*!< TCP mode. */
} eMBMode;


/* Errorcodes used by all function in the protocol stack.
used by every modbus related function, [by liq, 2019-11] */
typedef enum
{
    MB_ENOERR,                  /*!< no error. */
    MB_ENOREG,                  /*!< illegal register address. */
    MB_EINVAL,                  /*!< illegal argument. */
    MB_EPORTERR,                /*!< porting layer error. */
    MB_ENORES,                  /*!< insufficient resources. */
    MB_EIO,                     /*!< I/O error. */
    MB_EILLSTATE,               /*!< protocol stack in illegal state. */
    MB_ETIMEDOUT                /*!< timeout error occurred. */
} eMBErrorCode;



/* register should be written or read.
 * This value is passed to the callback functions which support either
 * reading or writing register values. Writing means that the application
 * registers should be updated and reading means that the modbus protocol
 * stack needs to know the current register values. see eMBRegHoldingCB( ), 
 * eMBRegCoilsCB( ), eMBRegDiscreteCB( ) and  eMBRegInputCB( ).
 used by all mb lib files. [by liq, 2019-11]*/
typedef enum
{
    MB_REG_READ,                /*!< Read register values and pass to protocol stack. */
    MB_REG_WRITE                /*!< Update register values. */
} eMBRegisterMode;


/* message event code, send form port level to mb_poll(). 
only 'EV_FRAME_RECEIVED' is used. [by liq, 2019-11]*/
typedef enum
{
    EV_READY,                   /*!< Startup finished. */
    EV_FRAME_RECEIVED = 1,      /*!< Frame received. */
    EV_EXECUTE,                 /*!< Execute function. */
    EV_FRAME_SENT               /*!< Frame sent.*/
} eMBEventType;


/* port funtion type defintion, 
used in 'MB_SLAVE_STRU' below, [by liq, 2019-11] */
/* below is in port/event */
typedef int32_t (* tp_event_init)(void);
typedef int32_t (* tp_event_post)(uint32_t e);
typedef int32_t (* tp_event_get)(uint32_t * e);
/* below is in port/serial */
typedef int32_t (* tp_serial_init)( uint8_t port, uint32_t baudrate, uint8_t databits, uint8_t parity);
typedef void    (* tp_serial_enable)(uint32_t enrx, uint32_t entx);
typedef void    (* tp_serial_start_send)(uint8_t d[], int n);
typedef int32_t (* tp_serial_read_receive)(uint8_t d[], int n);
typedef int32_t (* tp_serail_check_TC)(void);
typedef int32_t (* tp_serial_check_IDLE)(void);

/* below is in port/tcp */
typedef int32_t (* tp_tcpsvr_init)(uint16_t tcpport);
typedef void    (* tp_tcpsvr_enable)(uint32_t en);
typedef int32_t (* tp_tcpsvr_receiving)(uint8_t d[], uint16_t * len);
typedef int32_t (* tp_tcpsvr_send)(uint8_t d[], uint16_t len);

/* below is in port/timer */
typedef int32_t (* tp_timer_init)(uint32_t n_50us);
typedef void    (* tp_timer_enable)(uint32_t en);
/* below is in modbus/rtu/ascii/tcp */
typedef int32_t (* tp_slave_init)(void *slave);
typedef void    (* tp_slave_enable)(void *slave, int32_t en);
typedef int32_t (* tp_slave_receive_pdu)(void *slave, uint8_t * pucRcvAddress, uint8_t ** pucFrame, uint16_t * pusLength);
typedef int32_t (* tp_slave_send_pdu)(void *slave, uint8_t ucSlaveAddress, const uint8_t * pucFrame, uint16_t usLength );



/* this is a data asmembly of a slave */
typedef struct 
{
    /* below is the basic config parameters */
    uint8_t                 state;
    uint8_t                 address;                        //slave's address, 1~247
    uint8_t                 mode;                           //slave mode RTU/ASCII/TCP
    uint16_t                port_id;                        //eg. 2=uart2
    uint32_t                baudrate;                       //eg. 115200
    uint8_t                 databits;
    uint8_t                 parity;
    
    /* below is in port/event */
    tp_event_init           p_event_init; 
    tp_event_post           p_event_post;
    tp_event_get            p_event_get;

    /* below is in port/serial */
    tp_serial_init          p_serial_init;
    tp_serial_enable        p_serial_enable;
    tp_serial_start_send    p_serial_start_send;
    tp_serial_read_receive  p_serial_read_receive;
    tp_serail_check_TC      p_serial_check_TC;
    tp_serial_check_IDLE    p_serial_check_IDLE;

    /* below is in port/tcp*/
    tp_tcpsvr_init          p_tcpsvr_init;
    tp_tcpsvr_enable        p_tcpsvr_enable;
    tp_tcpsvr_receiving     p_tcpsvr_receiving;
    tp_tcpsvr_send          p_tcpsvr_send;

    /* below is in port/timer */
    tp_timer_init           p_timer_init;
    tp_timer_enable         p_timer_enable;

    /* below is in modbus/rtu */
    tp_slave_init           p_slave_init; 
    tp_slave_enable         p_slave_enable;
    tp_slave_receive_pdu    p_slave_receive_pdu;    
    tp_slave_send_pdu       p_slave_send_pdu;
    

    /* below is the processing data. */
    uint8_t*                p_pdu;                          //pointer to store the pdu, actuall point to ucRTUBuf[1]
    uint8_t                 function_code;                  //store the recent rx pdu's function code
    uint16_t                pdu_len;                        //store the recent rx pdu's length, also the tx pdu's length, it is multi-used, not for cnt up use in parse().
    uint8_t                 targetaddr;                     //store the recent rx pdu's target address
    uint8_t                 ucRTUBuf[256 + 8];              //pdu buf for rx and tx, the real data pool, the adu(addr, func-code, data, err-check), and 8 byte for tcp's MBAP (need 7 byte only).

    /* below is for ascii only */
    uint8_t                 *p_ascii_txbuf;                 //frame buf for ascii tx.
    //uint16_t                ascii_txbuf_len;                //valid data len in ascii_txbuf.
    uint8_t                 ascii_rcv_state;                //receive byte parse state
    uint16_t                idx_rtubuf;                     //receive byte index, in ucRTUBuf.
    uint8_t                 e_nibble;                       //the high or low part of a byte

    //..below is statistics data for a slave
    uint32_t                receive_ok_cnt;                 //receive good function code from master, addup in mbpoll()
    uint32_t                receive_input_cnt;              //this slave's input register is accessed, cnt of time
    uint32_t                receive_hold_cnt;               //this slave's hold  register is accessed, cnt of time
    uint32_t                receive_other_cnt;              //other register(coils or descrete) is accessed.
    
} MB_SLAVE_STRU;


/* below is the public function for user, from mb.c */
int32_t mb_init  ( MB_SLAVE_STRU *slave );
int32_t mb_enable( MB_SLAVE_STRU *slave , int newstate);
int32_t mb_poll  ( MB_SLAVE_STRU *slave );



#ifdef __cplusplus
}
#endif
#endif

