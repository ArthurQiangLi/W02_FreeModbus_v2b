/**
  ******************************************************************************
  * @file    TASK HEADER FILE: public info of MB1(rtu) and MB5(tcp)
  * @author  arthur.qiang.li
  * @version V1
  * @date    2021/11/07
  * @brief   none
  *                      
  ******************************************************************************
  */
/*******************************************************************************
********************* Define to prevent recursive inclusion ********************
*******************************************************************************/

#ifndef _TASK_MB_H
#define _TASK_MB_H

/*******************************************************************************
************************************ Includes **********************************
*******************************************************************************/
#include <stdint.h>

/*******************************************************************************
********************************* Exported types *******************************
*******************************************************************************/

                                                            //* call back type definition, for updata event of holding register, used in mbapp. */
typedef void (*MB_HOLD_UPDATE_CB)(uint16_t usAddress, uint16_t usNRegs);


                                                            //...data for other task to use, which is a subset of TASK_XXX_PRIVATE_STRU
typedef struct {
    uint32_t        typeid;                                 //device typeid
    uint32_t        receive_ok_cnt;                         //rxcnt of the key data frame
    uint32_t        link_level;
    uint32_t        receive_input_cnt;
    uint32_t        receive_hold_cnt;
    uint32_t        receive_other_cnt;
    
} TASK_MB_READ_MAIN_DATA_STRU;

/*******************************************************************************
********************************  Cfgs and Consts  *****************************
*******************************************************************************/
                                                            //cfg task period time in ms
#define CFG_TASK_MB_PERIOD_MS           (20)


                                                            //*define read() request code*/
typedef enum {
    TASK_MB_READ_CNT       = 1,                             //read task runcnt, in u32 hex format
    TASK_MB_READ_MAIN_DATA = 2,                             //read task main data, for agv, app read it to interface_stru.
    
} TASK_MB_READ_ENUM;


                                                            //*define ioc() request code*/
typedef enum {
    TASK_MB_IOC_DEBUGMODE = 1,                              //set debug mode for this thread.
    TASK_MB_IOC_SET_HOLD_CB,                                //for both mb1/5, set hold cb to mb stack
    TASK_MB_IOC_GET_INPUT_PTR,                              //get input register ptr
    TASK_MB_IOC_GET_HOLD_PTR,                               //get hold register ptr
    TASK_MB_IOC_UPDATE_MBHOLD  = 100,                       //mbapp call update the mbhold of this task
    TASK_MB_IOC_UPDATA_MBINPUT = 200,                       //mbapp call update the mbinput of this task
    
} TASK_MB_IOC_ENUM;

/*******************************************************************************
******************************* Exported functions *****************************
*******************************************************************************/
                                                            //for mb1
extern void     task_mb1_start  (int32_t prio, int32_t delayms, int32_t en);
extern int32_t  task_mb1_read   (uint32_t request, void * pv);
extern int32_t  task_mb1_ioc    (uint32_t req, void *pd);
                                                            //for mb5
extern void     task_mb5_start  (int32_t prio, int32_t delayms, int32_t en);
extern int32_t  task_mb5_read   (uint32_t request, void * pv);
extern int32_t  task_mb5_ioc    (uint32_t req, void *pd);

#endif /* _TASK_MB_H */

/********************************* end of file ********************************/



