/**
  ******************************************************************************
  * @file    module of mb protocol implementation
  * @author  arthur.qiang.li
  * @version V2
  * @date    2019.1017
  * @brief   V1 = freemodbus 2010/06/06 13:54:40 wolti.
  *
  ******************************************************************************
  */

/*******************************************************************************
************************************ Includes **********************************
*******************************************************************************/

#include <stdint.h>
#include "mb.h"
#include "mbconfig.h"
#include "mbframe.h" /* use macro definition */
#include "mbproto.h" /* for xMBFunctionHandler[] */
#include "mbfunc.h" /* for xMBFunctionHandler[] */

#if MB_RTU_ENABLED == 1
    #include "mbrtu.h"
#endif
#if MB_ASCII_ENABLED == 1
    #include "mbascii.h"
#endif
#if MB_TCP_ENABLED == 1
    #include "mbtcp.h"
#endif

/*******************************************************************************
******************************** Private define ********************************
*******************************************************************************/
/* slave's state, used in init(), enable(), poll() of this module. */
typedef enum
{
    STATE_NOT_INITIALIZED,
    STATE_ENABLED,
    STATE_DISABLED,
} MB_SLAVE_STATE_ENUM;


/*******************************************************************************
******************************* Private variables ******************************
*******************************************************************************/

/* An array of Modbus functions handlers which associates Modbus function
codes with implementing functions. */
static xMBFunctionHandler mb_function_handlers[MB_FUNC_HANDLERS_MAX] = 
{
#if MB_FUNC_OTHER_REP_SLAVEID_ENABLED > 0
    {MB_FUNC_OTHER_REPORT_SLAVEID, eMBFuncReportSlaveID},
#endif
#if MB_FUNC_READ_INPUT_ENABLED > 0
    {MB_FUNC_READ_INPUT_REGISTER, eMBFuncReadInputRegister}, 
#endif

#if MB_FUNC_READ_HOLDING_ENABLED > 0
    {MB_FUNC_READ_HOLDING_REGISTER, eMBFuncReadHoldingRegister}, 
#endif

#if MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED > 0
    {MB_FUNC_WRITE_MULTIPLE_REGISTERS, eMBFuncWriteMultipleHoldingRegister}, 
#endif

#if MB_FUNC_WRITE_HOLDING_ENABLED > 0
    {MB_FUNC_WRITE_REGISTER, eMBFuncWriteHoldingRegister}, 
#endif

#if MB_FUNC_READWRITE_HOLDING_ENABLED > 0
    {MB_FUNC_READWRITE_MULTIPLE_REGISTERS, eMBFuncReadWriteMultipleHoldingRegister},
#endif

#if MB_FUNC_READ_COILS_ENABLED > 0
    {MB_FUNC_READ_COILS, eMBFuncReadCoils}, 
#endif

#if MB_FUNC_WRITE_COIL_ENABLED > 0
    {MB_FUNC_WRITE_SINGLE_COIL, eMBFuncWriteCoil},
#endif

#if MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED > 0
    {MB_FUNC_WRITE_MULTIPLE_COILS, eMBFuncWriteMultipleCoils},
#endif

#if MB_FUNC_READ_DISCRETE_INPUTS_ENABLED > 0
    {MB_FUNC_READ_DISCRETE_INPUTS, eMBFuncReadDiscreteInputs},
#endif
};


/*******************************************************************************
  * @brief  mb slave init
  *
  * @param  slave = the slave instance, eg. a rtu instance.
  *
  * @retval 0= OK, other= error.
  *
  * @note   [by liq, 2019-11]
  *****************************************************************************/
int32_t mb_init(MB_SLAVE_STRU *slave)
{
    
    int32_t  e; /*temp used*/

    /* check slave struct's every element. */
    /* check first state, after mcu starts it is zero */
    if(slave->state != STATE_NOT_INITIALIZED){
        return __LINE__; 
    }

    /* check slave address */
    if( ( slave->address == MB_ADDRESS_BROADCAST ) ||
        ( slave->address < MB_ADDRESS_MIN ) || ( slave->address > MB_ADDRESS_MAX ) )
    {
        return __LINE__;
    }
    
    switch(slave->mode){
        case MB_RTU:
            slave->p_slave_init        = (tp_slave_init)mb_rtu_init;
            slave->p_slave_enable      = (tp_slave_enable)mb_rtu_enable;
            slave->p_slave_send_pdu    = (tp_slave_send_pdu)mb_rtu_send_pdu;
            slave->p_slave_receive_pdu = (tp_slave_receive_pdu)mb_rtu_receive_pdu;
            break;
        
        case MB_ASCII:
#if MB_ASCII_ENABLED > 0

        slave->p_slave_init        = (tp_slave_init)mb_ascii_init;
            slave->p_slave_enable      = (tp_slave_enable)mb_ascii_enable;
            slave->p_slave_send_pdu    = (tp_slave_send_pdu)mb_ascii_send_pdu;
            slave->p_slave_receive_pdu = (tp_slave_receive_pdu)mb_ascii_receive_pdu;
            break;
#endif        
        case MB_TCP:
            slave->p_slave_init        = (tp_slave_init)mb_tcp_init;
            slave->p_slave_enable      = (tp_slave_enable)mb_tcp_enable;
            slave->p_slave_send_pdu    = (tp_slave_send_pdu)mb_tcp_send_pdu;
            slave->p_slave_receive_pdu = (tp_slave_receive_pdu)mb_tcp_receive_pdu;
            break;
        
        default:
            return __LINE__;
    }

    /* init low level rtu hardware. */
    e = slave->p_slave_init(slave); 
    if(e != 0){
        return __LINE__;
    }

    /* to here, everything is ok. */
    slave->state= STATE_DISABLED;

    return 0;
}


/*******************************************************************************
  * @brief  mb slave enable/disable, calls actually eg. rtu_enable()
  *
  * @param  slave, and newstate(0/1)     
  *
  * @retval 0= OK.
  *
  * @note   none
  *****************************************************************************/
int32_t mb_enable( MB_SLAVE_STRU *slave , int newstate)
{
    /*check input parameter*/
    if(newstate != 0 && newstate != 1){
        return __LINE__;
    }

    /*check last state, if to enable, last state must be disabled. */
    if(newstate == 1 && slave->state != STATE_DISABLED){
        return __LINE__;
    }
    
    if( newstate == 1 )                                     /*to enalbe*/
    {
        slave->p_slave_enable(slave, 1);/* Activate the protocol stack. */
        slave->state = STATE_ENABLED;
    }
    else{                                                   /*to disable*/
        
        if(slave->state == STATE_DISABLED){
            return __LINE__;
        }
        
        slave->p_slave_enable(slave, 0);
        slave->state = STATE_DISABLED;
    }


    return 0;
}


/*******************************************************************************
  * @brief  slave request polling process
  *
  * @param  the slave instance
  *
  * @retval err, 0=ok, other=not enable, pdu err, addr not match, , 
  *
  * @note   none
  *****************************************************************************/
int32_t mb_poll( MB_SLAVE_STRU *slave )
{
    static eMBException exception;
    
    uint32_t get_event;

    /* Check if the protocol stack is ready. */
    if( slave->state != STATE_ENABLED )
    {
        return __LINE__;
    }

    /* Check if there is a event available. If not return control to caller.
     * Otherwise we will handle the event. */
    if( slave->p_event_get( &get_event ) == 0 )
    {
        int32_t e; /* receive pdu err. */
        int32_t i;

        
        if(get_event == EV_FRAME_RECEIVED){
            /* check received string, and got the addr, pdu, len of pdu*/
            e = slave->p_slave_receive_pdu(slave, &(slave->targetaddr), &(slave->p_pdu), &(slave->pdu_len) );
            if(e != 0){
                return __LINE__; /* we has the 'received' message, but not got the pdu */
            }

            /* check if the new received frame is for this slave. */
            if(slave->targetaddr != slave->address
            && slave->targetaddr != MB_ADDRESS_BROADCAST ){
                return __LINE__;                            //* the pdu is not for this slave or broadcasting */
            }

            
            /* here, the frame is for this slave, prepare the response or exception. */
            slave->function_code = slave->p_pdu[MB_PDU_FUNC_OFF];
            exception = MB_EX_ILLEGAL_FUNCTION;
            for( i = 0; i < MB_FUNC_HANDLERS_MAX; i++ )
            {
                /* seek to table's end, and No more function handlers registered. Abort seeking. */
                if( mb_function_handlers[i].ucFunctionCode == 0 ){
                    break;
                }
                else if( mb_function_handlers[i].ucFunctionCode == (slave->function_code))
                {
                    //we have found the target function code
                    slave->receive_ok_cnt++;
                    if(slave->function_code==MB_FUNC_READ_INPUT_REGISTER){//func=04
                        slave->receive_input_cnt++;
                    }
                    else if(slave->function_code==MB_FUNC_READ_HOLDING_REGISTER ||  //func=03
                            slave->function_code==MB_FUNC_WRITE_REGISTER||//func=06
                            slave->function_code==MB_FUNC_WRITE_MULTIPLE_REGISTERS){//func=16
                        slave->receive_hold_cnt++;
                    }else{
                        slave->receive_other_cnt++;
                    }
                    
                    //here, the handle may call eg. eMBRegHoldingCB(), which will modify p_pdu and pdu_len 
                    exception = mb_function_handlers[i].pxHandler( slave->p_pdu, &(slave->pdu_len) );
                    break;
                }
            }

            /* defaulty, If the request was not sent to the broadcast address we return a reply. */
            if( slave->targetaddr != MB_ADDRESS_BROADCAST )
            {
                if( exception != MB_EX_NONE )
                {
                    /* An exception occured. Build an error frame. */
                    slave->pdu_len = 0;
                    slave->p_pdu[slave->pdu_len++] = ( uint8_t )( (slave->function_code) | MB_FUNC_ERROR );
                    slave->p_pdu[slave->pdu_len++] = exception;
                }
                                
                e = slave->p_slave_send_pdu(slave, slave->address, slave->p_pdu, slave->pdu_len ); 
                return 0;
            }
            
        }
    }
    return 0;//get event err.
}


/*******************************************************************************
******************************* Private functions ******************************
*******************************************************************************/


/*******************************************************************************
  * @brief  register function to xFucnHandlers
  *
  * @param  function code and and the handler     
  *
  * @retval none
  *
  * @note   [2017-5-31 by liq]
            this is global fucntion for all slave instance.
  *****************************************************************************/
eMBErrorCode mb_register_function( uint8_t f_code, pxMBFunctionHandler pxHandler )
{
    int             i;
    eMBErrorCode    eStatus;

    if( ( 0 < f_code ) && ( f_code <= 127 ) )
    {
        if( pxHandler != 0 )
        {
            for( i = 0; i < MB_FUNC_HANDLERS_MAX; i++ )
            {
                if( ( mb_function_handlers[i].pxHandler == 0 ) ||
                    ( mb_function_handlers[i].pxHandler == pxHandler ) )
                {
                    mb_function_handlers[i].ucFunctionCode = f_code;
                    mb_function_handlers[i].pxHandler = pxHandler;
                    break;
                }
            }
            eStatus = ( i != MB_FUNC_HANDLERS_MAX ) ? MB_ENOERR : MB_ENORES;
        }
        else
        {
            for( i = 0; i < MB_FUNC_HANDLERS_MAX; i++ )
            {
                if( mb_function_handlers[i].ucFunctionCode == f_code )
                {
                    mb_function_handlers[i].ucFunctionCode = 0;
                    mb_function_handlers[i].pxHandler = 0;
                    break;
                }
            }
            /* Remove can't fail. */
            eStatus = MB_ENOERR;
        }
    }
    else
    {
        eStatus = MB_EINVAL;
    }
    return eStatus;
}

/********************************* end of file ********************************/

