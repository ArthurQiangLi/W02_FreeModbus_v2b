/**
  ******************************************************************************
  * @file    inplementation methods of mb, mb register parsing.
  * @author  arthur.qiang.li
  * @version V1
  * @date    2021-11-04
  * @brief   

             Q: how do you design this module?
             A: 

             Q: how to use these methods?
             A: 

*******************************************************************************/
/*******************************************************************************
************************************ Includes **********************************
*******************************************************************************/
//---call some lib---
#include "cmsis_os.h"

//---call some lowlevel---
//---call other task/module---

//---call local module---
#include "mb.h"
#include "mbconfig.h"
#include "mbutils.h"

#include "./mb_method.h"
#include "./mb_private_cfg.h"                             


/*******************************************************************************
******************************** Private define ********************************
*******************************************************************************/
/* slave's address bias, default are all zero */
const uint16_t   usSDiscInStart   = 0;
const uint16_t   usSCoilStart     = 0;
const uint16_t   usSRegInStart    = 0;
const uint16_t   usSRegHoldStart  = 0;

/*******************************************************************************
******************************** Private typedef *******************************
*******************************************************************************/


/*******************************************************************************
******************************* Private variables ******************************
*******************************************************************************/

/* salve's variables */
#if MB_APP_DISC_NUM%8
    static uint8_t          ucSDiscInBuf[MB_APP_DISC_NUM/8+1];
#else
    static uint8_t          ucSDiscInBuf[MB_APP_DISC_NUM/8];
#endif

#if MB_APP_COIL_NUM%8
    static uint8_t          ucSCoilBuf[MB_APP_COIL_NUM/8+1];
#else
    static uint8_t          ucSCoilBuf[MB_APP_COIL_NUM/8];
#endif
                                                            //database of all mb channels.
static uint16_t             input_registers[MB_APP_INPUT_REG_NUM];
static uint16_t             hold_registers[MB_APP_HOLDING_REG_NUM];


                                                            //callback in eMBRegHoldingCB, executed when any hoding reg changes */
static MB_HOLD_UPDATE_CB    cb_mb_hold_updated;
/*******************************************************************************
************************* Private function declaration *************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
*                              Public functions                                *
********************************************************************************
*******************************************************************************/

/*******************************************************************************
  * @brief  other module/task call this to register a cb, set to eMBRegHoldingCB()
            in this file.
  *
  * @param  the address of the fucntion     
  *
  * @retval 0=no err.
  *
  * @note   none
  *****************************************************************************/
int32_t mb_set_holdupdate_callback(MB_HOLD_UPDATE_CB cb)
{
    if(cb == 0)
        return __LINE__;
    
    cb_mb_hold_updated = cb;
    return 0;
}

/*******************************************************************************
  * @brief  for extern to get register start pointer.
  *
  * @param  none
  *
  * @retval the array pointer.
  
  * @notte  none
  *****************************************************************************/
uint16_t *mb_get_hold_ptr(void)
{
    return hold_registers;
}


uint16_t *mb_get_input_ptr(void)
{
    return input_registers;
}


/*******************************************************************************
  * @brief  eMBFuncReadInputRegister() call this, 
            this is slave input register callback function. 
  *
  * @param  pucRegBuffer, input register buffer
            usAddress, input register address
            usNRegs, input register number
  *
  * @retval none
  
  * @notte  used in only one place, and is declared as extern
  *****************************************************************************/ 
eMBErrorCode eMBRegInputCB(uint8_t * pucRegBuffer, uint16_t usAddress, uint16_t usNRegs )
{
    eMBErrorCode      eStatus = MB_ENOERR;
    uint16_t          iRegIndex;
    uint16_t *        pusRegInputBuf;
    uint16_t          REG_INPUT_START;
    uint16_t          REG_INPUT_NREGS;
    uint16_t          usRegInStart;

    pusRegInputBuf  = input_registers;
    REG_INPUT_START = MB_APP_INPUT_REG_START_ADDR;
    REG_INPUT_NREGS = MB_APP_INPUT_REG_NUM;
    usRegInStart    = usSRegInStart;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= REG_INPUT_START)
            && (usAddress + usNRegs <= REG_INPUT_START + REG_INPUT_NREGS))
    {
        iRegIndex = usAddress - usRegInStart;
        while (usNRegs > 0)
        {
            *pucRegBuffer++ = (uint8_t) (pusRegInputBuf[iRegIndex] >> 8);
            *pucRegBuffer++ = (uint8_t) (pusRegInputBuf[iRegIndex] & 0xFF);
            iRegIndex++;
            usNRegs--;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }

    return eStatus;
}


/*******************************************************************************
  * @brief  used only in mbfucntioholding.c and is declared as extern.
            this is slave holding register callback function.
  *
  * @param  pucRegBuffer, holding register buffer
            usAddress, holding register address, it starts form 1. 
            usNRegs, holding register number 
            eMode, read or write 
  *
  * @retval the result
  
  * @notte  none
  *****************************************************************************/ 
eMBErrorCode eMBRegHoldingCB(uint8_t * pucRegBuffer, uint16_t usAddress, uint16_t usNRegs, eMBRegisterMode eMode)
{
    eMBErrorCode    eStatus = MB_ENOERR;
    uint16_t          iRegIndex;
    uint16_t *        pusRegHoldingBuf;
    uint16_t          REG_HOLDING_START;
    uint16_t          REG_HOLDING_NREGS;
    uint16_t          usRegHoldStart;
    uint16_t          num_to_callback;
    
    num_to_callback = usNRegs; /* data transmitted to the callback. */

    pusRegHoldingBuf  = hold_registers;
    REG_HOLDING_START = MB_APP_HOLDING_REG_START_ADDR;
    REG_HOLDING_NREGS = MB_APP_HOLDING_REG_NUM;
    usRegHoldStart    = usSRegHoldStart;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= REG_HOLDING_START)
            && (usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS))
    {
        iRegIndex = usAddress - usRegHoldStart;
        switch (eMode)
        {
        /* read current register values from the protocol stack. */
        case MB_REG_READ:
            while (usNRegs > 0)
            {
                *pucRegBuffer++ = (uint8_t) (pusRegHoldingBuf[iRegIndex] >> 8);
                *pucRegBuffer++ = (uint8_t) (pusRegHoldingBuf[iRegIndex] & 0xFF);
                iRegIndex++;
                usNRegs--;
            }
            break;

        /* write current register values with new values from the protocol stack. */
        case MB_REG_WRITE:
            while (usNRegs > 0)
            {
                pusRegHoldingBuf[iRegIndex] = *pucRegBuffer++ << 8;
                pusRegHoldingBuf[iRegIndex] |= *pucRegBuffer++;
                iRegIndex++;
                usNRegs--;
            }
            
            /* updata holdings, call the cb function, [by liq, 2019-10] */
            if(cb_mb_hold_updated != 0){
                /* to here, the usAddress is a num, started from 0, not 1.*/
                cb_mb_hold_updated(usAddress, num_to_callback);
            }
            break;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
}


/*******************************************************************************
  * @brief  called only in mbfunctioncoils.c, this is slave coils callback function.
  *
  * @param  pucRegBuffer, coils buffereu
            sAddress, coils addressu
            sNCoils, coils numbere
            Mode, read or write
  * @retval result
  
  * @notte  not used, 2021/11/07 liq.
  *****************************************************************************/
eMBErrorCode eMBRegCoilsCB(uint8_t * pucRegBuffer, uint16_t usAddress, uint16_t usNCoils, eMBRegisterMode eMode)
{
    eMBErrorCode    eStatus = MB_ENOERR;
    uint16_t          iRegIndex , iRegBitIndex , iNReg;
    uint8_t *         pucCoilBuf;
    uint16_t          COIL_START;
    uint16_t          COIL_NCOILS;
    uint16_t          usCoilStart;
    iNReg =  usNCoils / 8 + 1;

    pucCoilBuf  = ucSCoilBuf;
    COIL_START  = MB_APP_COIL_START_ADDR;
    COIL_NCOILS = MB_APP_COIL_NUM;
    usCoilStart = usSCoilStart;

    /* it already plus one in modbus function method. */
    usAddress--;

    if( ( usAddress >= COIL_START ) &&
        ( usAddress + usNCoils <= COIL_START + COIL_NCOILS ) )
    {
        iRegIndex = (uint16_t) (usAddress - usCoilStart) / 8;
        iRegBitIndex = (uint16_t) (usAddress - usCoilStart) % 8;
        switch ( eMode )
        {
        /* read current coil values from the protocol stack. */
        case MB_REG_READ:
            while (iNReg > 0)
            {
                *pucRegBuffer++ = xMBUtilGetBits(&pucCoilBuf[iRegIndex++],
                        iRegBitIndex, 8);
                iNReg--;
            }
            pucRegBuffer--;
            /* last coils */
            usNCoils = usNCoils % 8;
            /* filling zero to high bit */
            *pucRegBuffer = *pucRegBuffer << (8 - usNCoils);
            *pucRegBuffer = *pucRegBuffer >> (8 - usNCoils);
            break;

            /* write current coil values with new values from the protocol stack. */
        case MB_REG_WRITE:
            while (iNReg > 1)
            {
                xMBUtilSetBits(&pucCoilBuf[iRegIndex++], iRegBitIndex, 8,
                        *pucRegBuffer++);
                iNReg--;
            }
            /* last coils */
            usNCoils = usNCoils % 8;
            /* xMBUtilSetBits has bug when ucNBits is zero */
            if (usNCoils != 0)
            {
                xMBUtilSetBits(&pucCoilBuf[iRegIndex++], iRegBitIndex, usNCoils,
                        *pucRegBuffer++);
            }
            break;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
}


/*******************************************************************************
  * @brief  called only in mbfucntiondisc.c. this is slave discrete callback fucntion
  *
  * @param  pucRegBuffer, discrete buffer 
            usAddress, discrete address 
           usNDiscrete, discrete number 
  *
  * @retval none
  
  * @notte  is used and declared as extern in the used file,
            not used. 2021/11/07 liq
  *****************************************************************************/
eMBErrorCode eMBRegDiscreteCB( uint8_t * pucRegBuffer, uint16_t usAddress, uint16_t usNDiscrete )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    uint16_t          iRegIndex , iRegBitIndex , iNReg;
    uint8_t *         pucDiscreteInputBuf;
    uint16_t          DISCRETE_INPUT_START;
    uint16_t          DISCRETE_INPUT_NDISCRETES;
    uint16_t          usDiscreteInputStart;
    iNReg =  usNDiscrete / 8 + 1;

    pucDiscreteInputBuf       = ucSDiscInBuf;
    DISCRETE_INPUT_START      = MB_APP_DISC_START_ADDR;
    DISCRETE_INPUT_NDISCRETES = MB_APP_DISC_NUM;
    usDiscreteInputStart      = usSDiscInStart;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= DISCRETE_INPUT_START)
            && (usAddress + usNDiscrete    <= DISCRETE_INPUT_START + DISCRETE_INPUT_NDISCRETES))
    {
        iRegIndex = (uint16_t) (usAddress - usDiscreteInputStart) / 8;
        iRegBitIndex = (uint16_t) (usAddress - usDiscreteInputStart) % 8;

        while (iNReg > 0)
        {
            *pucRegBuffer++ = xMBUtilGetBits(&pucDiscreteInputBuf[iRegIndex++],
                    iRegBitIndex, 8);
            iNReg--;
        }
        pucRegBuffer--;
        /* last discrete */
        usNDiscrete = usNDiscrete % 8;
        /* filling zero to high bit */
        *pucRegBuffer = *pucRegBuffer << (8 - usNDiscrete);
        *pucRegBuffer = *pucRegBuffer >> (8 - usNDiscrete);
    }
    else
    {
        eStatus = MB_ENOREG;
    }

    return eStatus;
}





/********************************* end of file ********************************/

