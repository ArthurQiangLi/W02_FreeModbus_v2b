/**
  ******************************************************************************
  * @file    modbus protocol task, polling task for slave #01 separately, so
             you can use msg to make the mb more real time.
  * @author  arthur.qiang.li
  * @version V2
  * @date    V1 2018-03-10
  * @brief   V2 23 Feb 2020
  *                      
  ******************************************************************************
  */

//---call some lib---
#include "cmsis_os.h"
#include <stdint.h>
//---call some lowlevel---
#include "bsp_tp.h"
#include "bsp_led.h"

//---call some task/module---
#include "mb.h"
#include "./cli_log_mb.h"
#include "./mb_method.h"

/*******************************************************************************
******************************** cfg this task  ********************************
*******************************************************************************/

#define CFG_TASK_MBPOLL_PERIOD_MS   (10)
#define CFG_TASK_MBPOLL_HZ          (100)

#define MB_PORT_EVENT_USE_QUEUE     (0)                     //0=not use, 1=use queue, no delay

#define MBPOLL_USE_BSP_TP           (0)                     //0=disable, 1=enable, if use bsp_tp module for testing and measuring

/*******************************************************************************
******************************** Private define ********************************
*******************************************************************************/

#if (MBPOLL_USE_BSP_TP == 1)
    #define TP_HI(pin)              bsp_tp_high(pin)
    #define TP_LO(pin)              bsp_tp_low(pin)
#else
    #define TP_HI(pin)      
    #define TP_LO(pin)      
#endif

                                                            //*private public data of this task */
typedef struct
{                                                         
    uint32_t    runcnt;                                     //task loop run cnt
    uint32_t    cfg_debug_mode;                             //0=normal, 1=debug

    uint32_t    hzcnt;                                      //cnt for 1 hz reset;
    uint32_t    rxcnt_last;
    uint32_t    rxhz;
    uint32_t    link_level;

} TASK_MB1_PRIVATE_STRU;     

/*******************************************************************************
******************************* Private variables ******************************
*******************************************************************************/
static TASK_MB1_PRIVATE_STRU        mb1_stru;
extern MB_SLAVE_STRU                mb_slave_rtu_01;

/*******************************************************************************
************************* Private function declaration *************************
*******************************************************************************/
static void     mb1_loop            (void const * argument);
static void     mb1_update_mbhold   (uint16_t v[]);
static void     mb1_update_mbinput  (uint16_t reg[]);

/*******************************************************************************
********************************************************************************
*                              public functions                                *
********************************************************************************
*******************************************************************************/

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
void task_mb1_start(int prio, int delayms, int en)
{
                                                            //* USER CODE BEGIN */
    const char *    name        = "mb1";                    //task name string
    os_pthread      thread      = mb1_loop;                 //task main body like void task_stdout(void const * argument)
    void *          arg         = (void*)delayms;           //param to the task
    int             stacksize   = 256;                      //stack size in word
    osThreadId      id;                                     //return id.
                                                            //* USER CODE END */
    
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


/*******************************************************************************
  * @brief  your task call this to get data form this module.
  *
  * @param  request, the code for what to read
            pd, where you store the result data.
  *
  * @retval 0= no error.
  * @notte  none
  
  * @notte  how to use?
        uint32_t cnt;
        TASK_XXX_STRU data;
        
        task_xxx_read(TASK_XXX_READ_CNT, &cnt);
        task_xxx_read(TASK_XXX_READ_DATA_STRU, &data);
  
  *****************************************************************************/
int32_t task_mb1_read(uint32_t request, void * pd)
{
    if(pd == 0)
        return -1;                                          //param error, should not be a null pointer
        
    switch(request){
                                                            //read task runcnt
        case TASK_MB_READ_CNT:
            *(uint32_t *)pd = mb1_stru.runcnt;              //convert pd to a (u32 *) pointer and use * to access the target
            break;
        case TASK_MB_READ_MAIN_DATA:
            if(pd != 0){
                TASK_MB_READ_MAIN_DATA_STRU *ooo;
                ooo                    = pd;
                ooo->typeid            = 6002;              //device typeid
                ooo->receive_ok_cnt    = mb_slave_rtu_01.receive_ok_cnt;//rxcnt of the key data frame
                ooo->link_level        = mb1_stru.link_level;
                ooo->receive_input_cnt = mb_slave_rtu_01.receive_input_cnt;
                ooo->receive_hold_cnt  = mb_slave_rtu_01.receive_hold_cnt;
                ooo->receive_other_cnt = mb_slave_rtu_01.receive_other_cnt;
            }
            break;            

        default:
            break;
    }

    return 0;
}


/*******************************************************************************
  * @brief  extern task call this to set info to this task.
  *
  * @param  none     
  *
  * @retval error, 0=no error, other is err.
  *
  * @note   none
  how to use?
  task_xxx_ioc(TASK_XXX_IOC_DEBUGMODE, (void *)1);
  *****************************************************************************/
int32_t task_mb1_ioc(uint32_t req, void *pd)
{
    int err = 0;                                            //default err=0
    
    switch(req){
                                                            //wether to block 'write' function
        case TASK_MB_IOC_DEBUGMODE: 
            break;

        case TASK_MB_IOC_SET_HOLD_CB:{
            MB_HOLD_UPDATE_CB cb = (MB_HOLD_UPDATE_CB)pd;
            err = mb_set_holdupdate_callback(cb);
            
        }
            break;

        case TASK_MB_IOC_GET_HOLD_PTR:{
            if(pd==0){
                err = __LINE__;
            }else{
                uint16_t *p;
                p = mb_get_hold_ptr();
                *(uint16_t **)pd = p;
            }
        }
            break;
        
        case TASK_MB_IOC_GET_INPUT_PTR:{
            if(pd==0){
                err = __LINE__;
            }else{
                uint16_t *p;
                p = mb_get_input_ptr();
                *(uint16_t **)pd = p;
            }
        }
            break;
            
        case TASK_MB_IOC_UPDATE_MBHOLD:{                    //*** write hold to this task.
            if(pd==0){
                err = __LINE__;
            }
            else{
                mb1_update_mbhold(pd);                      //pd is converted to a 'uint16_t v[]' pointer
            }
    
        }
        break;    

        case TASK_MB_IOC_UPDATA_MBINPUT:{                   //*** read input from this task.
            if(pd==0){
                err = __LINE__;
            }
            else{
                mb1_update_mbinput(pd);                     //pd is converted to a 'uint16_t v[]' pointer
            }
        }
        break;  


        default:
            break;
    }
    return err;
}


/*******************************************************************************
********************************************************************************
*                              private functions                               *
********************************************************************************
*******************************************************************************/

/*******************************************************************************
  * @brief  task main body
  * @param  the delay time.     
  * @retval none
  * @note   23 Feb 2020
  *****************************************************************************/
void mb1_loop(void const * argument)
{
    int32_t err;
                                                            //*time delay before start*/
    osDelay((int)argument);
    LOG(CLI_LOG_LEVEL_USR, "task 'mb1' starts.");           //show after osdelay(arg)
                                                            
    mb_init(&mb_slave_rtu_01);                              // in mb.c, load set of rtu/ascii/tcp, and call init
    mb_enable(&mb_slave_rtu_01, 1);                         //start slave
        
    for(;;)
    {
#if (MB_PORT_EVENT_USE_QUEUE != 1)        
        osDelay(CFG_TASK_MBPOLL_PERIOD_MS);
#endif                                                            
        TP_HI(5);
        err = mb_poll(&mb_slave_rtu_01); 
        TP_LO(5);
        mb1_stru.runcnt++;

        mb1_stru.hzcnt++;                                   //cntup for ONE sec.
        if(mb1_stru.hzcnt > CFG_TASK_MBPOLL_HZ){            //...here is executed every ONE second.
            mb1_stru.hzcnt      = 0;
            mb1_stru.rxhz       = mb_slave_rtu_01.receive_ok_cnt - mb1_stru.rxcnt_last;//calc hz
            mb1_stru.rxcnt_last = mb_slave_rtu_01.receive_ok_cnt; //update it after use
            mb1_stru.link_level = mb1_stru.rxhz;
        }

        
        if(err){
            //LOG(CLI_LOG_ERR, "err=%d.", err);
            osDelay(CFG_TASK_MBPOLL_PERIOD_MS);
        }     
    }
}


/*******************************************************************************
  * @brief  mb app task call this to set cmd to this task.
  *
  * @param  v[], the updated hold register
  *
  * @retval none
  
  * @notte  callback called in mbapp_slave_on_new_hoding_write()
  *****************************************************************************/
static void mb1_update_mbhold(uint16_t v[])
{
    mb1_stru.cfg_debug_mode = v[0];                         //cfg debug mode, 0=normal, 1=mb debug.
}


/*******************************************************************************
  * @brief  mb app task call this to refresh input registers for this task.
  *
  * @param  the input reg[] base
  *
  * @retval update the reg contents
  
  * @notte  none
  *****************************************************************************/
static void mb1_update_mbinput(uint16_t reg[])
{
    reg[0]  = mb1_stru.runcnt;
    reg[1]  = mb_slave_rtu_01.receive_input_cnt;
    reg[2]  = mb_slave_rtu_01.receive_hold_cnt;
    reg[3]  = mb_slave_rtu_01.receive_other_cnt;
    reg[4]  = mb_slave_rtu_01.receive_ok_cnt;
}

/********************************* end of file ********************************/

