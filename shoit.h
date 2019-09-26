/*
Copyright (c) 2019 Pencode.net
Author: StoneLi  <stone@bzline.cn> 

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#ifndef SHOIT_H__ 
#define SHOIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <strings.h>
#include <semaphore.h>
#include <pthread.h>



#define VERSION "shoit version is V0.99.090731.01"
#define LIMIT_FILE_SIZE_BYTES  8200000000
#define SEND_RATE 100000
#define MTU 1360
#define TIMEOUT (1*30)
#define SIZEOFFILENAME 256


#define USEC(st, fi) (((fi)->tv_sec-(st)->tv_sec)*1000000+((fi)->tv_usec-(st)->tv_usec))

typedef struct st_shoit_callbacks_t shoit_callbacks_t;


typedef enum {
    false=0,
    true=1
}bool;


typedef enum                                                                                                                         
{                                                                                                                                    
    CONNECT =1,   /*receiver-->sender, request connect and suceess*/                                                                             
    ROUND =2,   /*sender-->receiver,send some infomation to receiver*/                                                                   
    READY =3,     /*receiver-->send, notice sender, ready to sender*/                                                                             
    UDPEND =4,    /*sender -> receiver,notice receiver,send all packet in this round*/
    BITMAP =5,    /*receiver->sender,send the loss packet infomation to sender ...*/
    HEART =6,
    CLOSE =7      /*sender -> receiver, notice close all*/ 
}p2p_flow_status; 



/*
*make certain the offset of data[] is sizeof(shoit_control_t)
*/
#pragma pack (1)
typedef struct st_shoit_control_t{
    p2p_flow_status status;    
    uint16_t        iStream; /*0 or 1*/
    uint32_t        payloadSize;                                                                                                                 
    uint64_t        bucketSize;                                                                                                              
    uint64_t        fileSize; /*for mmap*/

    uint32_t        dataSize;
    char            data[];    /*maybe additional information*/ 
}shoit_control_t;


typedef struct st_shoit_udp_hdr{
    uint64_t seqno;
    uint32_t payloadSize;
    char payload[];    
}shoit_udp_hdr;
#pragma pack()  


typedef struct st_shoit_core_t{

    char *bucket; /*store the data, will be send or recv*/
    uint64_t bucketSize;

    size_t fileSize;/*for mmap*/
    size_t leftSize;/*for mmap*/
    size_t mapSize; /*for unmap*/

    uint32_t sendRate;/*Kbytes*/

    uint32_t payloadSize;
    uint32_t lastPayloadSize;

    uint32_t packetSize; /*udp packet, include payloadSIze+headerSize*/

    uint64_t totalNumberOfPackets;
    uint64_t remainNumberOfPackets;
    uint64_t receivedNumberOfPackets;
    uint64_t sentNumberOfPackets;

    uint32_t usecsPerPacket;

    int verbose;

    char *password; /*rece must use this password*/

    int  udpSockfd;
    int  tcpSockfd;

    char *bindHost; /*default 0.0.0.0*/
    char remoteHost[32];
    int  udpRemotePort;

    int  tcpPort;/*tcp server port*/
    int  udpLocalPort;
  
    
    struct sockaddr_in udpServerAddr;
    struct sockaddr_in tcpClientAddr;

    uint64_t *hashTable;
    char *errorBitmap;
    uint64_t sizeofErrorBitmap;
    bool isFirstBlast;


    FILE *log;

    /*data source and dest*/
    char *fileName; /*be send file name*/
    int fromFd;
    int toFd;

    /*thread synchronize*/
    sem_t progSem; /*progress sem*/
    sem_t dataSem; /*send or recv data sem*/
    int pipe[2];

    pthread_cond_t sendBlockCond;              // used to block "send" call
    pthread_mutex_t sendBlockLock;             // lock associated to m_SendBlockCond

    bool useMmap;
    bool iStream;
    bool runing;

    const shoit_callbacks_t  *callbacks;
    
}shoit_core_t;

typedef struct st_shoit_callbacks_t{
    /**
    * called for dataSource of sender 
    */
    bool  (*on_open_data)(shoit_core_t *shoit);
    void  (*on_close_data)(shoit_core_t *shoit);

    /**
    * called for dataSource of recv stream 
    */
    ssize_t  (*on_recv_data)(shoit_core_t *shoit,char *data,size_t len);

}shoit_callbacks_t;



////////////////////////////////function////////////


/*
*Functionality:
*   Seelp until CC "nexttime".
*Parameters:
*   [in] nexttime: next time the caller is waken up.                                                                                 
*Returned value:                                                                                                                     
*   None.                                                                                                                            
*/  
void shoit_sleepto(shoit_core_t *shoit,uint64_t nexttime);


void shoit_show_copyright();

void shoit_init_synch(shoit_core_t *shoit);
void shoit_destory_synch(shoit_core_t *shoit);


void shoit_init_error_bitmap(shoit_core_t *shoit);

/*
*set bitmap value base on seq
*if set error return false;
*/
bool shoit_update_error_bitmap(shoit_core_t *shoit,uint64_t seq);

/*
*return the count of errors
*if update =true, update table
*/
uint64_t shoit_update_hash_table(shoit_core_t *shoit,bool update);

void shoit_set_verbose(shoit_core_t *shoit, int v );





#ifdef __cplusplus
}
#endif

#endif
