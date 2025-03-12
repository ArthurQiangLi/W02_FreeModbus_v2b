/**
  ******************************************************************************
  * @file    mb on tcp port
  * @author  arthur.qiang.li
  * @version V1 
  * @date    2020-2-11 
  * @brief   
             can only accept only one client/master, one file, one port.
             event is a compatable using, so we do not use os_queue.

  ******************************************************************************
  */

/*******************************************************************************
************************************ Includes **********************************
*******************************************************************************/

//---call some lib---
#include "cmsis_os.h"
#include "lwip/api.h"
#include "string.h"// if you want to use 'memcpy'.

//---call some task/module---
#include "mb.h"      //data type of slave structure
#include "mbtcp.h"  //use define MB_TCP_BUF_SIZE
#include "bsp_tp.h"  // for test only.
#include "cli_log_mb.h"//for LOG();
#include "ethernetif.h" //to use netif_is_link_up() in ethernetif_notify_conn_changed()
#include "tcp.h"//to use keep_alive

/*******************************************************************************
******************************** Private define ********************************
*******************************************************************************/
//#define LOG(...)   

#define MB_TCP_SERVER_PORT             (502)                //port for this module, usually 502.

/*******************************************************************************
******************************** Private typedef *******************************
*******************************************************************************/
                                                            // private data of this module
typedef struct
{
    struct netconn  *lconn;                                 //listen conn
    struct netconn  *newconn;                               //modbus client conn, we hold one only.

    uint8_t         netif_is_up;                            //1=is up, 0=is down, show for other module
    uint8_t         is_initing;                             //if init is be initialzing, to avoid re-entering init(), 1 Mar 2020 by liq.
    uint32_t        init_enter_cnt;                         //the time which init() is called, not called ok.
    uint32_t        event_value;                            //the value of the message
    int32_t         event_is_valid;                         //0=invalid, 1=valid, whether there is a valid evernt in above 'store'
    int32_t         cnt_conn_changed;                       //cnt of notify_conn_changed() be called.
    int32_t         cnt_reinit;                             //(not total) cnt of reinit after receive() no work for mb,
    struct netbuf   *netbuf_rcv;                            //rcv netbuf used in receive function
    void            *rcvdata;                               //of this time, rcv data pointer, from netbuf_rcv
    uint16_t        rcvlen;                                 //of this time, rcv data len in rcvdata[]
    uint32_t        rcvlen_total;                           //sum of rcvlen, for showing.
    uint16_t        rcvbufpos;                              //static store the buf rcv index, from 0 to MB_TCP_BUF_SIZE
    uint16_t        rcvbufpos_last;                         //last value of rcvbufpos, for showing.
    uint32_t        rcvadu_total;                           //rcv num of adu, valid rcvbufpos, for showing.
    uint8_t         rcvbuf[MB_TCP_BUF_SIZE];                //buf to concatenate rcv data in to one complete adu,
                                                            //... actuall it is not need, we can see it as a copy of adu data.
}MBPORT_TCP_STRU;


                                                            /*//private data of this module,
                                                            data mts is not connected to the slave stru, the slave stru connects to
                                                            the functions in this file, while the functions connect data mts.*/
//static MBPORT_TCP_STRU     mts;
MBPORT_TCP_STRU     mts;
extern struct netif gnetif;                                 //netif in lwip.c the default ethernet_if on LAN8742


/*******************************************************************************
  * @brief  event init
  *
  * @param  None
  *
  * @retval 0= no error
  *****************************************************************************/
static int32_t event_init(void)
{
    mts.event_is_valid = 0;
    return 0;
}


/*******************************************************************************
  * @brief  send event
  *
  * @param  eEvent
  *
  * @retval 0= no error
  *****************************************************************************/
static int32_t event_post(uint32_t e)
{

    mts.event_is_valid  = 1;  
    mts.event_value     = e;  
    
    return 0;
}   


/*******************************************************************************
  * @brief  get event
  *
  * @param  *e
  *
  * @retval 0= no error
  *****************************************************************************/
static int32_t event_get(uint32_t * e)
{
    if(mts.event_is_valid){
        *e = mts.event_value;
        mts.event_is_valid = 0;
        return 0;
    }
    return -1;
}


static char * err_string(int e)
{
    char *r;
    
    switch(e){
    case ERR_OK         :/** = 0,No error, everything OK. */
        r="ERR_OK";
        break;
    case ERR_MEM        :/** = -1,    Out of memory error.     */
        r="ERR_MEM";
        break;
    case ERR_BUF        :/** = -2,    Buffer error.            */
        r="ERR_BUF";
        break;
    case ERR_TIMEOUT    :/** = -3,    Timeout.                 */
        r="ERR_TIMEOUT";
        break;

    case ERR_RTE        :/** = -4,    Routing problem.         */
        r="ERR_RTE";
        break;

    case ERR_INPROGRESS :/** = -5,    Operation in progress    */
        r="IN PROGRESS";
        break;

    case ERR_VAL        :/** = -6,    Illegal value.           */
        r="ILLEGAL VAL";
        break;

    case ERR_WOULDBLOCK :/** = -7,    Operation would block.   */
        r="WOULDBLOCK";
        break;

    case ERR_USE        :/** = -8,    Address in use.          */
        r="ADDR IN USE";
        break;

    case ERR_ALREADY    :/** = -9,    Already connecting.      */
        r="ALREADY CONN";
        break;

    case ERR_ISCONN     :/** = -10,    Conn already established.*/
        r="ISCONN";
        break;

    case ERR_CONN       :/** = -11,    Not connected.           */
        r="CONN";
        break;
    case ERR_IF         :/** = -12,    Low-level netif error    */
        r="NETIF ERR";
        break;
    case ERR_ABRT       :/** = -13,    Connection aborted.      */
        r="CONN ABORTED";
        break;
    case ERR_RST        :/** = -14,    Connection reset.        */
        r="CONN RST";
        break;
    case ERR_CLSD       :/** = -15,    Connection closed.       */
        r="CONN CLOSED";
        break;
    case ERR_ARG        :/** = -16    Illegal argument.        */
        r="ILLEGAL ARGUMENT";
        break;
    default:
        r="unknow?";
        break;

    }
    return r;
}


/*******************************************************************************
  * @brief  init tcp server, it listen and accept a new link then return.
  *
  * @param  portnum, if 0, use default
  *
  * @retval err, 0=no error
  * @notte  
  *****************************************************************************/
static int tcpserver_init(uint16_t tcpport)
{
    int terr;                                               //temp used err
    char *es;                                               //err string

    mts.is_initing = 1;                                     //prevent reentering
    mts.init_enter_cnt++;
                                                            //wait until eth-link is up.
    terr = 0;
    while(netif_is_link_up(&gnetif) == 0)
    { 
        LOG(CLI_LOG_ERR, "netif is not up, check it(n=%d)...", terr++);
        osDelay(2000);
    }

    //...build/rebuild listen-conn, only listen onetime,
    terr = netconn_delete(mts.lconn);                       //..first reset it, (delete = close and free, 'conn' becomes null)
    if(terr != ERR_OK){
        es = err_string(terr);
        LOG(CLI_LOG_ERR, "tcpinit delete conn err=%d, %s", terr, es);
        return __LINE__;
    }
    osDelay(100);

    mts.lconn = netconn_new( NETCONN_TCP );                 //..create/recreate it
    if(mts.lconn == 0){                                     //if creat failed
        LOG(CLI_LOG_ERR, "tcpinit creat listen conn error.");
        return __LINE__;                                    
    }
                                                            //..bind to socket, the lconn bind to "LOCAL:tcpport"
    if(tcpport == 0){                                       //if in param is 0, set it as default 502 port num.
        tcpport = MB_TCP_SERVER_PORT;
    }
    ip_set_option((mts.lconn->pcb.tcp), SOF_REUSEADDR);     //allow local address reuse,
    terr = netconn_bind(mts.lconn, NULL, tcpport);      
    if(terr != ERR_OK){
        es = err_string(terr);
        LOG(CLI_LOG_ERR, "bind conn error=%d, %s.", terr, es);
        return __LINE__;
    }

    terr = netconn_listen(mts.lconn);                       //..tell the conn to goto listening mode
    if(terr != ERR_OK){
        es = err_string(terr);
        LOG(CLI_LOG_ERR, "listen error=%d, %s.", terr, es);
        return __LINE__;
    }

    //...build/rebuild newconn
    terr = netconn_delete(mts.newconn);                     //..first reset it, (delete = close and free, 'conn' becomes null)
    osDelay(100);
    
    terr = netconn_accept(mts.lconn, &mts.newconn);         //start grabing a new connection
    if(terr != ERR_OK){
        es = err_string(terr);
        LOG(CLI_LOG_ERR, "asccept err=%d, %s.", terr, es);
        return __LINE__;
    }

    mts.newconn->recv_timeout = 1500;                       //set timeout for receive and listen, in ms.
    mts.rcvbufpos = 0;                                      //reset the buf index for following use
    mts.newconn->pcb.tcp->keep_idle  = 9000;
    mts.newconn->pcb.tcp->keep_intvl = 3000;
    mts.newconn->pcb.tcp->keep_cnt   = 5;
    event_post(EV_FRAME_RECEIVED);                          //post a event, otherwise will not able to go poll().

    return 0;                                               //no err. show we are succesful.
}

/*******************************************************************************
  * @brief  port io control
  *
  * @param  en = 0/1
  *
  * @retval shutdown any open client sockets
  *****************************************************************************/ 
static void tcpserver_enable(uint32_t en)
{
    if(en == 0){
      int err;
        
        err = netconn_delete(mts.newconn);
        if(err != 0){
            LOG(CLI_LOG_ERR, "netcon del(l=%d), ERR=%d.", __LINE__, err);
        } 
    }
    if(en == 1){
        //...nothing,
    }
}


/*******************************************************************************
  * @brief  notify user about link status changement
  *
  * @param  netif: the network interface
  *
  * @retval ** [1 Mar 2020 by liq.]
            ** this is a callback, exit as soon as possible.
  *****************************************************************************/   
void ethernetif_notify_conn_changed(struct netif *netif)
{  
    mts.cnt_conn_changed++;
    if(netif_is_link_up(netif))
    {
        mts.netif_is_up = 1;                                //show it is up
        LOG(CLI_LOG_ERR, "eth-link is ->UP [%d].", mts.cnt_conn_changed);
    }else{
        mts.netif_is_up = 0;                                //show it is down.
        LOG(CLI_LOG_ERR, "eth-link is ->DOWN [%d].", mts.cnt_conn_changed);
    }

}

/*******************************************************************************
  * @brief  blocking receive data.
  *
  * @param  d= output ADU pointer
            n= output ADU total len
  *
  * @retval erron code, 0=no error.
  * @notte  called by mbpoll(), pointer by 'p_slave_receive_pdu'
  *****************************************************************************/
static int tcpserver_receving( uint8_t d[], uint16_t *out_dlen )
{
    int e; //err_t enum,

    if(mts.newconn == NULL){
        LOG(CLI_LOG_ERR, "newconn is NULL in rcv.");
        osDelay(500);
        //e = tcpserver_reaccept();
        //LOG(CLI_LOG_USR, "rcv()/reaccept(),[e=%d].", e);        
        return __LINE__;
    }

    event_post(EV_FRAME_RECEIVED); //always post it, compatable with rtu/ascii port's event mechanism

    e = netconn_recv(mts.newconn, &(mts.netbuf_rcv));       //blocked before timeout.
    if(e != ERR_OK){
        char * es;//pointer for err string lookup,
        
        mts.cnt_reinit++;
        es = err_string(e);
        LOG(CLI_LOG_ERR, "rcv(), err=%d (%s).", e, es);
        if(e)
        {
            //ERR_TIMEOUT//ERR_ABRT//ERR_RST//ERR_CLSD
                osDelay(2000);
                e = tcpserver_init(0);
            LOG(CLI_LOG_USR, "tcpinit %d-th done, eline=%d.", mts.cnt_reinit, e);
        }
        return __LINE__;                                    //err, no data return.
    }

    //...now received some data in netbuf_rcv, fentch them in below.
    do{
        netbuf_data(mts.netbuf_rcv, &(mts.rcvdata), &(mts.rcvlen));
        mts.rcvlen_total += mts.rcvlen;                     //showing total cnt from port, raw data.
                                                            //store data to local zone. first check if there is enough space
        if((mts.rcvbufpos + mts.rcvlen) >= MB_TCP_BUF_SIZE){
            mts.rcvbufpos = 0;                              //sorry, no space for incoming data, will not copy to buf[SIZE], break and next delete the netbuf.
            netbuf_delete(mts.netbuf_rcv); 
            return __LINE__;                                //frame too big, exceed buf[SIZE].
        }
        else{
            memcpy(&(mts.rcvbuf[mts.rcvbufpos]), mts.rcvdata, mts.rcvlen);  
            mts.rcvbufpos += mts.rcvlen;
        }
    } 
    while (netbuf_next(mts.netbuf_rcv) >= 0);
    netbuf_delete(mts.netbuf_rcv);


    //...now we get the buf[SIZE], its content is less than SIZE, not overrun, and..let's parse it
    if(mts.rcvbufpos >= MB_TCP_FUNC)                        //make sure it is > 7 (MBAP len), the minimal len of a total ADU frame
    {
        uint16_t   usLength;                                //len in MBAP's LEN nibble.
        usLength  = mts.rcvbuf[MB_TCP_LEN] << 8U;           //ADU[4],[5], LEN is u16 type, LEN=1+1+N, 1-unit_id, 1-function code, n-data
        usLength |= mts.rcvbuf[MB_TCP_LEN + 1];

                                                            // Is the frame already complete
        if( mts.rcvbufpos < ( MB_TCP_UID + usLength ) ){ 
                                                            //if tatal len in buf[] is not reach 6+LEN, that's the data is not fully filled
                                                            //...we do nothing, wait next period.
            LOG(CLI_LOG_ERR, "rcv bytes (%d) not reach 6+LEN(%d).", mts.rcvbufpos, usLength); 
            return __LINE__;
        }
        else if(mts.rcvbufpos == (MB_TCP_UID + usLength)){
            
            memcpy(d, mts.rcvbuf, mts.rcvbufpos);  
            *out_dlen = mts.rcvbufpos;
            mts.rcvbufpos_last = mts.rcvbufpos;             //showing last bufpos
            mts.rcvadu_total++;                             //showing the adu cnt

            mts.rcvbufpos = 0;                              //reset the buffer
            //LOG(CLI_LOG_USR, "OK, rcvd a good frame[%d].", mts.rcvadu_total);
            return 0;
        }
        else
        {
            LOG(CLI_LOG_ERR, "rcv too many bytes (%d),drop client.\r\n", mts.rcvbufpos); //this should not happen.
            mts.rcvbufpos = 0;
            return __LINE__;
        }
    }
    else{
        return __LINE__;  //adu is not long enought, less than a header size.
    }
    
}

/*******************************************************************************
  * @brief  send data to client (the only one client, 'mts.newconn')
  *
  * @param  data , to send
  * @param  len , in bytes
  *
  * @retval none
  * @notte  we use buffer copy in this function, actually we could not use COPY.
    netconn_write(mts.newconn, mts.pdata, mts.len, NETCONN_COPY);
  *****************************************************************************/
static int tcpserver_send(uint8_t pucMBTCPFrame[], uint16_t usTCPLength )
{
    int e;
    struct netbuf   *TCPNetbuf;                             //* the send pbuf */
                                                            //* creat a new pbuf */
    TCPNetbuf = netbuf_new();
                                                            //* cache the reference to the data source */
    netbuf_ref( TCPNetbuf, pucMBTCPFrame, usTCPLength);
    
    e = netconn_write( mts.newconn, (void *)pucMBTCPFrame, usTCPLength, NETCONN_COPY/*NETCONN_NOCOPY*/ );
    if(e != ERR_OK){
        LOG(CLI_LOG_ERR, "net write err=%d.", e);
    }                                                       //* delete cache and data. */
    netbuf_delete( TCPNetbuf );

    return 0;
}



                                                            //public data of this module
MB_SLAVE_STRU mb_slave_tcp_05 =
{
                                                            /* cfg all the handler of the slave instance*/
    .address               = 0x05,                          //should be a legal value, or will be refused by mb_init()
    .mode                  = MB_TCP,
    .p_event_init          = event_init,
    .p_event_post          = event_post,
    .p_event_get           = event_get,
    .p_tcpsvr_init         = tcpserver_init,
    .p_tcpsvr_enable       = tcpserver_enable,
    .p_tcpsvr_send         = tcpserver_send,
    .p_tcpsvr_receiving    = tcpserver_receving,
};

/********************************* end of file ********************************/


#if 0

    USR_LOG("receive: len=%d|",  len);
    /* */   
    int i;
    
    for(i=0; i<len; i++){
        DBG_LOG("%02X ", d[i]);
    }
    //DBG_LOG("\r\n");
#endif


