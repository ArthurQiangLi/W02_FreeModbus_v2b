/**
  ******************************************************************************
  * @file    MODULE HEADER FILE: .c file only need to include this file to have LOG() fucntion.
  * @author  arthur.qiang.li
  * @version V1
  * @date    2020-2-3
  * @brief   none
  ******************************************************************************
  */

#ifndef __CLI_LOG_MB_H
#define __CLI_LOG_MB_H

#include <stdint.h>
#include "cli_logcfg.h" //the marco of ENABLE_COMPILE

/*******************************************************************************
******************************* Exported functions *****************************
*******************************************************************************/
                                                            //declaration, when you call LOG(), it is cli_log_xxx() actually.
extern int  cli_log_mb(int level, const char *fmt, ...);


                                                            //the host task can use LOG(...), that's so good.
#if (LOGCFG_COMPILE_ENABLE_MB)
    #define LOG(...)  cli_log_mb( __VA_ARGS__)              //here '...' will be replaced by '__VA_ARGS__'
#else
    #define LOG(...)
#endif

#ifndef LOGCFG_COMPILE_ENABLE_MB
    #err "missing definition of this marco."
#endif

#endif /* __CLI_LOG_MB_H */

/********************************* end of file ********************************/

