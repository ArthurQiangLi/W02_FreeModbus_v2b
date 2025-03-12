/**
  ******************************************************************************
  * @file    modbus protocol task, polling task for slave #03 separately, so
             you can use msg to make the mb more real time.
  * @author  arthur.qiang.li
  * @version V1
  * @date    V1 2018-03-10
  * @brief   V2 23 Feb 2020
  *                      
  ******************************************************************************
  */

//---call some lib---
#include "cmsis_os.h"
#include <stdint.h>

//---call some task/module---
#include "mb.h"
#include "mb_register.h" /*to use mb register varaiables and size definition */
#include "bsp_tp.h"
#include "bsp_led.h"
#include "cli_log_mb.h"
/*******************************************************************************
******************************** cfg this task  ********************************
*******************************************************************************/

#define CFG_TASK_MBPOLL_PERIOD_MS   (10)

#define MB_PORT_EVENT_USE_QUEUE     (1)                     //0=not use, 1=use queue, no delay

#define MBPOLL_USE_BSP_TP           (1)                     //0=disable, 1=enable, if use bsp_tp module for testing and measuring

/*******************************************************************************
******************************** Private define ********************************
*******************************************************************************/

#if (MBPOLL_USE_BSP_TP == 1)
    #define TP_HI(pin)      bsp_tp_high(pin)
    #define TP_LO(pin)      bsp_tp_low(pin)
#else
    #define TP_HI(pin)      
    #define TP_LO(pin)      
#endif


/*******************************************************************************
  * @brief  task main body
  * @param  the delay time.     
  * @retval none
  * @note   23 Feb 2020
  *****************************************************************************/
void mbpoll_loop_03(void const * argument)
{
    extern MB_SLAVE_STRU  mb_slave_ascii_03;
    int32_t err;
                                                            //*time delay before start*/
    osDelay((int)argument);
    LOG(CLI_LOG_LEVEL_USR, "task 'mb-loop-03' starts.");    //show after osdelay(arg)

    mb_init(&mb_slave_ascii_03);                            // in mb.c, load set of rtu/ascii/tcp, and call init
    mb_enable(&mb_slave_ascii_03, 1/*1=enable*/);           //start slave
        
    for(;;)
    {
#if (MB_PORT_EVENT_USE_QUEUE != 1)        
        osDelay(CFG_TASK_MBPOLL_PERIOD_MS);
#endif                                                            
        TP_HI(5);
        err = mb_poll(&mb_slave_ascii_03); 
        TP_LO(5);
        
        if(err){
            osDelay(CFG_TASK_MBPOLL_PERIOD_MS);
        }
#if 0        
        if(err == 0){
            static uint32_t cnt;
            
            cnt++;
            
            if(cnt % 10 == 0)
                bsp_led_toggle(1);
        }   
#endif        
    }
}



/*******************************************************************************
  * @brief  Creat and start a task
  *
  * @param  prio: task prority, find definition in cmsis_os.h
            delayms : param to the task
            en: 1=start task, 0=delete task
  *
  * @retval a pointer to the task TCB.
  *
  * @notte  none
  *****************************************************************************/
void task_mbpoll_03_start(int prio, int delayms, int en)
{
    /* USER CODE BEGIN */
    const char *    name        = "mbpoll_03";              //task name string
    os_pthread      thread      = mbpoll_loop_03;           //task main body like void task_stdout(void const * argument)
    void *          arg         = (void*)delayms;           //param to the task
    int             stacksize   = 256;                      //stack size in word
    osThreadId      id;                                     //return id.
    /* USER CODE END */
    
    const osThreadDef_t os_thread_def = { (char *)name, (os_pthread)thread, (osPriority)prio, 0, stacksize};
    
                                                            //to start the task
    if(en == 1){
        id = osThreadCreate(&os_thread_def, arg);
        if(id == NULL){
            while(1){
                LOG(CLI_LOG_ERR, "creating task '%s' failed.", name);
                osDelay(3000);
            }
        }    
    }else{
        if(id != NULL){
            osThreadTerminate(id);                          //delete the task.
        }
    }
}

/********************************* end of file ********************************/

