/**
  ******************************************************************************
  * @file    module: log function of err, usr, and dbg level for [all modbus] model
  * @author  arthur.qiang.li
  * @version V1
  * @date    1 mar 2020
  * @brief   log cfg manager is unified in cli_logcfg.c

  ******************************************************************************
  */

#include <stdint.h> //uint32_t
#include <stdio.h> //print
#include <stdarg.h>  //var_list
#include "cli_logcfg.h" //logcfg datatype
#include "cmsis_os.h"// osKernelSysTick()

/*******************************************************************************
*                                setting for this module                       *
*******************************************************************************/
                                                            //declare extern data of 'cli_logcfg_stru_##name'
EXTERN_CLI_LOGCFG_STRU(mb);

                                                            //define fmt string for printf(), for this module, change if you need a special outlook
#define FMT_ERR_STR(name)    "\r\n["#name" e%d]"
#define FMT_USR_STR(name)    "\r\n["#name" u%d]"

/*******************************************************************************
  * @brief  log function for the host task
  *
  * @param  level, a value of enum CLI_LOG_LEVEL_ENUM
            fmt and ...,  used as printf(fmt, ...)
  *
  * @retval err, 0=no err.
  *
  * @note   for "fmt", there is NO line feed management,
            you must end with a '\n' if you want a new line.
  *****************************************************************************/
int  cli_log_mb(int level, const char *fmt, ...)
{
    uint32_t   ts;
                                                            //first, check 'fmt'
    if(fmt == 0){
        return -1;
    }
                                                            //then, check 'level'
    switch(level){
                                                            //for err, start with a newline
        case CLI_LOG_LEVEL_ERR:
            if(CLI_LOGCFG_STRU(mb).en_err == 1){
                ts = osKernelSysTick();
                printf(FMT_ERR_STR(mb), ts);
                break;
            }else{
                return __LINE__;                            //will not printf for this level is disabled
            }                                      
                                                            //for usr, start with a newline
        case CLI_LOG_LEVEL_USR:
            if(CLI_LOGCFG_STRU(mb).en_usr == 1){
                ts = osKernelSysTick();
                printf(FMT_USR_STR(mb), ts);
                break;
            } else {
                return __LINE__;                            //will not printf for this level is disabled
            }
                                                            //for dbg, NOTHING added
        case CLI_LOG_LEVEL_DBG:
            if(CLI_LOGCFG_STRU(mb).en_dbg == 1){
                break;
            }
            else {
                return __LINE__;                            //will not printf for this level is disabled
            }

        default:
            return -2;                                      //other, error 'level'
    }
                                                            //the logging main part.
    {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);                                 //put string to stdout port
        va_end(args);
        return 0;                                           //return no error.
    }
}

/********************************* end of file ********************************/

