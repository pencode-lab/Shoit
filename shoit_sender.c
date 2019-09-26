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

#include "shoit_sender.h"
#include "shoit_misc.h"
#include "shoit_network.h"
#include "shoit_progress.h"

#define SHOIT_LOG(format, ...) do{ shoit_misc_log(sender->log,format,##__VA_ARGS__); } while(0)

static progress_t progress;


//////////////////static function//////////////////

static void _sender_run_loop(shoit_core_t *sender);
static void sender_free(shoit_core_t *sender);
static bool tcpserver_init(shoit_core_t *sender);                                                                                    
static void tcpserver_run(shoit_core_t *sender);
static bool connect_udp_server(shoit_core_t *sender);
static void blast_udp_sending(shoit_core_t *sender);
static void transfer_thread_run(void *);
static void progress_thread_run(void *);
static void catch_signal_init();
static void catch_signal(int sig);
static bool echo_to_receiver_on_tcp(shoit_core_t *sender, int type);
//////////////////////////





/*sender->bucketSize change,so change other...*/
void update_sender_bucket_infomation(shoit_core_t *sender)
{
    if (sender->bucketSize % sender->payloadSize == 0)                                                                               
    {                                                                                                                                
        sender->totalNumberOfPackets = sender->bucketSize/sender->payloadSize;                                                       
        sender->lastPayloadSize = sender->payloadSize;                                                                               
    }                                                                                                                                
    else                                                                                                                             
    {                                                                                                                                
        sender->totalNumberOfPackets = sender->bucketSize/sender->payloadSize + 1; /* the last packet is not full */                 
        sender->lastPayloadSize = sender->bucketSize - sender->payloadSize * (sender->totalNumberOfPackets - 1);                     
    }                                                                                                                                
    sender->remainNumberOfPackets = sender->totalNumberOfPackets;                                                                    
                                                                                                                                     
    /*this control send rate*/                                                                                                       
    sender->usecsPerPacket = 8 * sender->payloadSize * 1000 / sender->sendRate;                                                      
    sender->sizeofErrorBitmap = sender->totalNumberOfPackets/8 + 2;                                                                  

    if(sender->errorBitmap){
        free(sender->errorBitmap);
        sender->errorBitmap = NULL;
    } 
    sender->errorBitmap = (char *)malloc(sender->sizeofErrorBitmap+sizeof(shoit_control_t));                                                        
    if (sender->errorBitmap == NULL)
    {                                                   
        SHOIT_LOG("malloc error(%s)\n",strerror(errno));
        return;
    } 

    if(sender->hashTable){
        free(sender->hashTable);
        sender->hashTable = NULL;
    }
    sender->hashTable = (uint64_t *)malloc(sender->totalNumberOfPackets * sizeof(uint64_t));                                         
    if (sender->hashTable == NULL)                                                                                                   
    {                                                                                                                                
        SHOIT_LOG("malloc hashTable failed\n");                                                                                      
        return ;                                                                                                                     
    }                                                                                                                                

    sender->sentNumberOfPackets = 0;
    sender->isFirstBlast = true;
    for (uint64_t i=0; i<sender->totalNumberOfPackets; i++)
    {                            
        sender->hashTable[i] = i;
    }
    shoit_init_error_bitmap(sender);

                     
}

void sender_run_loop(shoit_core_t *sender,uint32_t sendRate,uint32_t packetSize)
{                                                                                                                                    

    catch_signal_init();
    shoit_init_synch(sender);

    memset(&progress,0,sizeof(progress_t));
    progress.total =0;

    sender->udpRemotePort = sender->tcpPort;
    sender->udpLocalPort = sender->udpRemotePort +1;

    /*ready for sender init
    *assignment value to sender                                                                                               
    */

    if(sender->callbacks->on_open_data(sender)==false){                                                                  
        SHOIT_LOG("callbacks open data storage return false,will return.");                                                                    
        return;                                                                                                                  
    }  

    int pageSize = (int)sysconf(_SC_PAGESIZE);//4096 or 8192..
    if(sender->iStream){
        sender->fileSize =0;/*stream,don't know file size*/
        sender->bucketSize = packetSize*pageSize*64;
        sender->bucket= (char *)malloc(sender->bucketSize);                                                                                  
        sender->useMmap=false;    

        if(sender->bucket==NULL){                                                                                                        
            SHOIT_LOG("bucket is NULL,error(%s)\n",strerror(errno));                                                                     
            return;                                                                                                                      
        }   

    }else if(sender->fileName!=NULL){
        uint64_t bucketSize = pageSize*packetSize*256*1;/*1G, pageSize 4096*/                                                        
        uint64_t fileSize = sender->fileSize;                                                                                        
        if(fileSize < bucketSize){                                                                                                       
            if(fileSize%pageSize)                                                                                                        
                bucketSize = fileSize;                                                                                                   
            else{                                                                                                                        
                bucketSize = pageSize*((fileSize/pageSize)+1);                                                                           
            }                                                                                                                            
            fileSize = bucketSize;                                                                                                       
        }                                                                                                                            
                                                                                                                                     
        sender->fileSize = fileSize;                                                                                                 
        sender->leftSize = sender->fileSize;                                                                                         
        sender->bucketSize = bucketSize;

        sender->bucket=NULL;                                                                                                         
        sender->useMmap=true; 

            
    }

    sender->sendRate = sendRate;
    sender->payloadSize =packetSize;
    sender->packetSize = sender->payloadSize + sizeof(shoit_udp_hdr);


    update_sender_bucket_infomation(sender);

    /*core loop*/
    _sender_run_loop(sender);                                                                           


    /*                                                                                                                               
    *free sender and return                                                                                                          
    */                                                                                                                               
    sender_free(sender);                                                                                                             
                                                                                                                                     
    return;                                                                                                                          
}           

///////////////////////////static func////////////////////////

static void _sender_run_loop(shoit_core_t *sender)
{
     /*                                                                                                                               
    * sender is ready, can start recv request of client(receiver)                                                                    
    */                                                                                                                               
                                                                                                                                     
    do{                                                                                                                              
        sender->runing = true;                                                                                                       
                                                                                                                                     
        /*blocking accept until client connect*/                                                                                     
        SHOIT_LOG("Start runing sender...wait the remote receiver connect.\n");
        if(!tcpserver_init(sender)){                                                                                                 
            SHOIT_LOG("tcpserver init failed.\n");                                                                                   
            break;                                                                                                                   
        }                                                                                                                            
                                                                                                                                     
        struct sockaddr_in *client =&sender->tcpClientAddr;                                                                          
        inet_ntop(AF_INET,&client->sin_addr, sender->remoteHost, sizeof(sender->remoteHost));                                        
                                                                                                                                     
        /*create data transfer thread base on udp*/                                                                                  
        pthread_t transfer_thid;                                                                                                     
        pthread_create(&transfer_thid,NULL,(void*)transfer_thread_run,(void*)sender);                                                
                                                                                                                                     
                                                                                                                                     
        /*create progress thread*/                                                                                                   
        pthread_t prog_thid;                                                                                                         
        pthread_create(&prog_thid,NULL,(void*)progress_thread_run,(void*)sender);                                                    
                                                                                                                                     
                                                                                                                                     
        /*run main control thread*/                                                                                                  
        tcpserver_run(sender);                                                                                              
                                                                                                                                     
        /*free thread*/                                                                                                              
        sem_post(&sender->dataSem);/*let us udp send data in man pthread*/                                                           
        sem_post(&sender->progSem);/*notice progress thread*/                                                                        
                                                                                                                                     
                                                                                                                                     
        /*wait thread exit*/                                                                                                         
        pthread_join(transfer_thid,NULL);                                                                                            
        pthread_join(prog_thid,NULL);                                                                                                
                                                                                                                                     
    }while(0); 


    return;

}




static void tcpserver_run(shoit_core_t *sender)
{
    struct timeval timeout;
    fd_set rset,orgSet;
    int maxfdpl;
    int done=0;
    int retval;

    shoit_control_t crlhdr; /*tcp header*/

    maxfdpl = sender->tcpSockfd +1;                                                                                          
    FD_ZERO(&orgSet);                                                                                                                
    FD_SET(sender->tcpSockfd, &orgSet);

    while(!done && sender->runing){
        timeout.tv_sec = 5;                                                                                                          
        timeout.tv_usec = 0;                                                                                                         
        memcpy(&rset,&orgSet,sizeof(orgSet));                                                                                        
        retval = select(maxfdpl, &rset, NULL, NULL, &timeout);                                                                       
        if(retval<0){/*error*/                                                                                                       
            SHOIT_LOG("select return -1 (%s) at 0xe01\n",strerror(errno));                                                         
            done =1;                                                                                                                 
        } if(retval>0 && FD_ISSET(sender->tcpSockfd, &rset)){          
            memset(&crlhdr,0,sizeof(shoit_control_t));

            if(shoit_network_readn(sender->tcpSockfd,
                                  (char*)&crlhdr,sizeof(shoit_control_t))==sizeof(shoit_control_t)) {                                   
                switch(crlhdr.status){                                                                                              
                    case CONNECT:/*receiver request*/                                                                                
                        if(sender->verbose>1) SHOIT_LOG("CONNECT signal\n");                                               
                        char password[16];bzero(password,sizeof(password));
                        int dataSize = ntohl(crlhdr.dataSize);
                        if(dataSize>0){
                            if(shoit_network_readn(sender->tcpSockfd,password,dataSize)!=dataSize){
                                SHOIT_LOG("read line for password error");
                                done =1;
                                break;
                            }
                        }
                        /*check password*/
                        if(strncmp(password,sender->password,dataSize-sizeof(int))){
                            SHOIT_LOG("remote password[%s]:[%s] is wrong.\n",password,sender->password);
                            done =1;
                            break;
                        }
                         /*notice pthread start transfer data to remote*/
                        sem_post(&sender->dataSem);/*let us udp send data in man pthread*/
                        sem_post(&sender->progSem);
                        if(sender->verbose>1) SHOIT_LOG("recv CONNECT signal and sem_post() start pthread\n");
                        break;                                                                                                       
                    case READY:
                        if(sender->verbose>1) SHOIT_LOG("get READY sig.and post sem.\n");
                        sem_post(&sender->dataSem);
                        break;
                    case BITMAP: /*receiver send me the errorBitMap*/                                                                
                        if(sender->verbose>1) SHOIT_LOG("BITMAP signal\n");
                        int needReadLen = ntohl(crlhdr.dataSize);
                        if(needReadLen ==0) {/*no error*/
                            char *errorBitmap = (char*)(sender->errorBitmap+sizeof(shoit_control_t));
                            errorBitmap[0]=1;
                        }else {
                             shoit_network_readn(sender->tcpSockfd,
                                                    (char*)(sender->errorBitmap+sizeof(shoit_control_t)),
                                                    needReadLen);
                        }
                        /*notic pthread process errorBitMap*/
                        if(sender->verbose>1) SHOIT_LOG("get BITMAP sig.and post sem.\n");
                        sem_post(&sender->dataSem);/*let us udp send data in man pthread*/
                        break;                                                                                                       
                    default:                                                                                                         
                        SHOIT_LOG("read error tcpcrl,ignore the recv data,error status[%d]. 0xe03\n",crlhdr.status);                                  
                        break;                                                                                                       
                }      
            }else{
                if(sender->verbose>1) SHOIT_LOG("remote socket close.\n");
                done =1;
            }
        }//retval >0
    }//while(!done)

    sender->runing = false;
    return;
}


static bool each_round_fetch_bucket(shoit_core_t *sender)
{

    size_t nread =0;
    bool retval=false;

    do{
        if(sender->useMmap) {

            if(sender->leftSize<=0) { retval =false; break;}

            size_t mapSize = (sender->leftSize/sender->bucketSize > 0) ? sender->bucketSize: sender->leftSize; 
            off_t offset= sender->bucketSize *((sender->fileSize-sender->leftSize)/sender->bucketSize); 
    

            sender->bucket= (char *)mmap(NULL,mapSize, PROT_READ, MAP_SHARED, sender->fromFd,offset); 
            if(sender->bucket==NULL){
                SHOIT_LOG("Mmap failed. %s\n",strerror(errno));
                retval = false;
                break;
            }
            sender->bucketSize = mapSize;
            sender->mapSize = mapSize;
            sender->leftSize -= mapSize;

            retval =true; /*first*/
            break; 
        }

        /*for stream*/
        nread = shoit_network_readn(sender->fromFd,sender->bucket,sender->bucketSize);
        if(nread<=0) break;
        if(nread!=sender->bucketSize){/*need recount the sender's parameter*/
            sender->bucketSize = nread;

            if (sender->bucketSize % sender->payloadSize == 0)                                                                               
            {                                                                                                                                
                sender->totalNumberOfPackets = sender->bucketSize/sender->payloadSize;                                                       
                sender->lastPayloadSize = sender->payloadSize;                                                                               
            }                                                                                                                                
            else                                                                                                                             
            {                                                                                                                                
                sender->totalNumberOfPackets = sender->bucketSize/sender->payloadSize + 1; /* the last packet is not full */                 
                sender->lastPayloadSize = sender->bucketSize - sender->payloadSize * (sender->totalNumberOfPackets - 1);                     
            }                                                                                                                                
            sender->sizeofErrorBitmap = sender->totalNumberOfPackets/8 + 2;                                                                  

        }
        retval = true;

    }while(0);
    
    return retval;
}


static bool each_round_transfer_init(void *arg)
{
    shoit_core_t *sender = (shoit_core_t*)arg;
    bool iMore=false;

    /*                                                                                                                               
    *from fromFd fetch data to sender->bucket again.                                                                                 
    *if send file and use mmap,don't need.                                                                                           
    */                                                                                                                               
    iMore = each_round_fetch_bucket(sender);   
    if(iMore){ 
        //reboot bucketSize change
        update_sender_bucket_infomation(sender);
    }
    return iMore;
}


static bool transfer_data_round_circle(shoit_core_t *sender)
{
    int done = 0;
    struct timeval curTime, startTime; 
    bool iMore;

    if(sender->verbose>1) SHOIT_LOG("call %s:0\n",__func__);

    /*
    * here need init this round variable,eg: sender->bucket...
    */
    iMore = each_round_transfer_init((void*)sender);
    if(!iMore) {
        if(sender->verbose>1) SHOIT_LOG("No more bucket to be send.\n");
        return iMore;
    }

    /*send buck info to reciever*/ 
    echo_to_receiver_on_tcp(sender,ROUND); 


    sem_wait(&sender->dataSem);/*be wakup:READY signal*/

    gettimeofday(&startTime, NULL);                                                                                                    
    while(iMore && !done && sender->runing){                                                                                                  
        if(sender->verbose>1) SHOIT_LOG("sending UDP packets");                                                                      
        blast_udp_sending(sender); 
        gettimeofday(&curTime, NULL);                                                                                                
                                                                                                                                     
        // send end of UDP signal                                                                                                    
        if(sender->verbose>1) SHOIT_LOG("send to socket %d an end signal.", sender->tcpSockfd);                    
        if(!echo_to_receiver_on_tcp(sender,UDPEND))
            SHOIT_LOG("send to socket %d an end signal error.", sender->tcpSockfd);                                                  
            
        // receive error list                                                                                                        
        sem_wait(&sender->dataSem); /*be wakeup:BITMAP signal*/
        char *errorBitmap = (char*)(sender->errorBitmap + sizeof(shoit_control_t));                                                  
        if ((unsigned char)errorBitmap[0] == 1)
        {
            done = 1;                                                                                                                
            sender->remainNumberOfPackets = 0;                                                                                       
            sender->sentNumberOfPackets = sender->totalNumberOfPackets;                                                              
            if(sender->mapSize>0){
                munmap(sender->bucket, sender->mapSize);
            }

        } else {
            /*
            Congestion Control Algorithms:
            The congestion control is optional.  The algorithm is
            if (lossRate > 0) {
                Rnew = Rold * (0.95 – lossRate);
            }
            */
            sender->remainNumberOfPackets = shoit_update_hash_table(sender,true);/*important!! update hashTable*/                    

            double lossRate = (double)sender->remainNumberOfPackets / (double)sender->totalNumberOfPackets;
            sender->usecsPerPacket = (int) ((double)sender->usecsPerPacket / (1.0 - lossRate - 0.05));
            SHOIT_LOG("loss rate = %.2f, and update sendrate =%d.",lossRate,sender->usecsPerPacket);

        }   
    }   


    if(!iMore){
        SHOIT_LOG("No more bucket will be send.\n");
    }
    return iMore;

}


static void transfer_thread_run(void *argv)
{
    shoit_core_t *sender = (shoit_core_t *)argv; 
    int done = 0;

    if(sender->runing){
        if(!connect_udp_server(sender)){
            SHOIT_LOG("connect udp server failed.\n");
            return;
        }
    }
    sem_wait(&sender->dataSem); /*be wakeup:CONNECT signal*/

    /*
    *here,you can send many bucket data in many round
    *if sendfile and mmap ,only one bucket
    */
    while(!done && sender->runing){
        if(transfer_data_round_circle(sender) == false) break; /*no more*/
    }


    /*send notice to remote:i will close*/
    echo_to_receiver_on_tcp(sender,CLOSE);
    sender->runing=false;

    pthread_exit(0);
   
}


static void blast_udp_sending(shoit_core_t *sender)
{
    int  done;
    uint64_t i;
    struct timeval start, now;
    char *msg = (char *) malloc(sender->packetSize);  //include sizeof(shoit_udp_hdr)  
    shoit_udp_hdr *udphdr=(shoit_udp_hdr *)msg;

    done = 0; i = 0;
    gettimeofday(&start, NULL);
    while(!done && sender->runing){

        memset(msg,0,sender->packetSize);
        gettimeofday(&now, NULL);
        uint64_t interval = USEC(&start, &now);

        /*control sendRate*/
        if(interval < sender->usecsPerPacket * i){
            while (interval < sender->usecsPerPacket * i){
                shoit_sleepto(sender,shoit_misc_get_timer()+interval/(sender->usecsPerPacket*8));                                                        
                gettimeofday(&now, NULL);                                                                                        
                interval = USEC(&start, &now);                                                                                   
            }                                                                                                                        
        } 
        /*send a packet*/
        else {
            int payloadSize;                                                                                                         
                                                                                                                                     
            udphdr->seqno = sender->hashTable[i];                                                                              
            if (udphdr->seqno < sender->totalNumberOfPackets - 1)                                                              
                payloadSize = sender->payloadSize;                                                                           
            else
                payloadSize = sender->lastPayloadSize;                                                                       
            udphdr->payloadSize = htonl(payloadSize);                                                                                   

            uint64_t offset = (uint64_t)(udphdr->seqno*sender->payloadSize);                                                 
            memcpy(udphdr->payload,(char*)((char*)sender->bucket+offset),payloadSize);         
            if (sendto(sender->udpSockfd, msg, sizeof(shoit_udp_hdr)+payloadSize, 0,                                               
                        (const struct sockaddr *)&sender->udpServerAddr, sizeof(sender->udpServerAddr)) < 0 ){        

                SHOIT_LOG("sendto error(%s)\n",strerror(errno));                                                                               
                sender->runing  = false;
            }else {                                                                                                                        
                i++;                                                                                                                     
                sender->sentNumberOfPackets++;                                                                                   
                progress.total += sizeof(shoit_udp_hdr)+payloadSize;
                if (i >= sender->remainNumberOfPackets)
                    done = 1;    
            }
        }

    }//while
    free (msg);

    return;
} 

static void progress_thread_run(void *argv)
{
    shoit_core_t *sender = (shoit_core_t *)argv;
    int done=0;
    int prog=0;
    struct timeval curTime, startTime;


    if(sender->verbose >1) SHOIT_LOG("start progress thread and block sem wait.\n");
    sem_wait(&sender->progSem);
    if(sender->verbose >1) SHOIT_LOG("start progress thread and unblock sem wait.runing[%d]\n",sender->runing);
    if(!sender->runing) pthread_exit(0);


    if(sender->iStream){
        shoit_progress_init(&progress, "", 100, PROGRESS_NUM_STYLE);
        SHOIT_LOG("Mbit/s      Sent(Kbytes)  time(s)\n") ;

        gettimeofday(&startTime, NULL);
        while(!done && sender->runing){
            gettimeofday(&curTime, NULL); 
            progress.utime = (curTime.tv_sec - startTime.tv_sec) + 1e-6*(curTime.tv_usec - startTime.tv_usec);
            shoit_progress_for_stream(sender->log,&progress,0);  
            usleep(1000000);

        }
        
    }else{
        shoit_progress_init(&progress, "", 100, PROGRESS_NUM_STYLE);
        SHOIT_LOG("Mbit/s      sent/total(Kbytes)  time(s)     progress\n") ;
        uint64_t timeout=0;
        gettimeofday(&startTime, NULL);
        while(!done && sender->runing){

            prog = (float)progress.total/ (float) sender->fileSize * 100;
            gettimeofday(&curTime, NULL);
            progress.utime =(curTime.tv_sec - startTime.tv_sec) + 1e-6*(curTime.tv_usec - startTime.tv_usec);
            shoit_progress_show_all(sender->log,&progress,(int)prog/100.0f,(long)(sender->fileSize >>10));       
            usleep(1000000);
            if(((curTime.tv_sec - startTime.tv_sec)/TIMEOUT )> timeout ){
                timeout  = (curTime.tv_sec - startTime.tv_sec)/TIMEOUT;
                echo_to_receiver_on_tcp(sender,HEART);
            }
                                                                                                                                     
        }       
    }


    gettimeofday(&curTime, NULL);                                                                                                    
    SHOIT_LOG("\n");                                                                                                            
    {//show complete info                                                                                                            
                                                                                                                                     
        float dt = (curTime.tv_sec - startTime.tv_sec) +                                                                             
                 1e-6*(curTime.tv_usec - startTime.tv_usec - 1000000);                                                                         
        uint64_t sent = progress.total; 
        float mbps = 1e-6 * 8*sent / (dt == 0 ? .01 : dt);
        if(sender->iStream)
            SHOIT_LOG("\nComplete transfer %dK in %.2fs (%g Mbit/s)",(sent>>10), dt, mbps);                                                  
        else
            SHOIT_LOG("\nComplete transfer %d/%dK in %.2fs (%g Mbit/s)",(sent>>10),(sender->bucketSize>>10), dt, mbps); 
        
    }                                                                                                                                

    shoit_progress_destroy(&progress);
    pthread_exit(0);
}


static void sender_free(shoit_core_t *sender)
{
    if(sender->bucket !=NULL){
        if(sender->useMmap){
            ;//munmap(sender->bucket, sender->mapSize); 
        }else{
            free(sender->bucket);
        }
        sender->bucket=NULL;
    } 
    sender->callbacks->on_close_data(sender);
    /*
    if(sender->fromFd !=-1) {
        close(sender->fromFd);
        sender->fromFd = -1;
    }
    */
    if(sender->toFd !=-1){
        close(sender->toFd);
        sender->toFd =-1;
    }
    

    if(sender->bindHost) {
        free(sender->bindHost);
        sender->bindHost=NULL;
    }

    if(sender->errorBitmap){    
        free(sender->errorBitmap);
        sender->errorBitmap =NULL;
    }
    if(sender->hashTable){
        free(sender->hashTable);
        sender->hashTable =NULL;
    }

    //free syn
    shoit_destory_synch(sender);

    //last free log
    if(sender->verbose>0) SHOIT_LOG("free sender complete.\n");
    shoit_misc_close_log(sender->log);
}



static bool tcpserver_init(shoit_core_t *sender)
{

    if(sender->verbose>1)
        SHOIT_LOG("call %s, bindhost[%s:%d]\n",__func__,sender->bindHost,sender->tcpPort);
    /*return connefd ,error return -1*/
    sender->tcpSockfd = shoit_network_tcp_server_init(sender->bindHost,sender->tcpPort,&sender->tcpClientAddr);
    SHOIT_LOG("Have client connect.....\n");
    return (sender->tcpSockfd ==-1)?false:true;
} 

static bool connect_udp_server(shoit_core_t *sender)
{
    /**as udp client,connect udp server*/
    if(sender->verbose>1)
        SHOIT_LOG("call %s udpLocalPort:%d,remoteudp :%s:%d\n",__func__,sender->udpLocalPort,sender->remoteHost, sender->udpRemotePort);
    sender->udpSockfd = shoit_network_connect_udp(&sender->udpServerAddr,sender->udpLocalPort,
                            sender->remoteHost, sender->udpRemotePort);

    return (sender->udpSockfd==-1)?false:true;

}

static void catch_signal(int sig)
{
    if(sig == SIGPIPE){
        fprintf(stderr,"catch SIGPIPE.\n");
    }   
}

static void catch_signal_init()
{
    signal(SIGPIPE,catch_signal);
}

static bool echo_to_receiver_on_tcp(shoit_core_t *sender, int type)
{
    char buffer[MTU]={0};
    int dataSize =0;
    shoit_control_t *tcpCrl=(shoit_control_t *)buffer;
    memset(buffer,0,sizeof(buffer));
    tcpCrl->status =type;
    tcpCrl->iStream = sender->iStream;

    if(sender->verbose>1) SHOIT_LOG("call %s:%d\n",__func__,type);

    switch(type){
        case ROUND:
        {
            tcpCrl->payloadSize = htonl(sender->payloadSize);
            tcpCrl->bucketSize = shoit_misc_htonll(sender->bucketSize);                                                                      
            tcpCrl->fileSize = shoit_misc_htonll(sender->fileSize);

            if(tcpCrl->iStream)
                tcpCrl->dataSize = htonl(0);                                                                                                  
            else if(sender->fileName!=NULL){
                char *notIncludePathFileName = shoit_misc_from_path_get_filename(sender->fileName);                                        
                if(notIncludePathFileName==NULL) {
                    SHOIT_LOG("echo_to_receiver_on_tcp error(0x010)\n");
                    return false;
                }
                dataSize = strlen(notIncludePathFileName);                                                                                  
                tcpCrl->dataSize = htonl(dataSize);                                                                                                    
                memcpy(tcpCrl->data,notIncludePathFileName,dataSize);
            } 

        }break;
        case UDPEND:
        case HEART:
        case CLOSE:
            tcpCrl->payloadSize = htonl(sender->payloadSize);
            tcpCrl->bucketSize = shoit_misc_htonll(sender->bucketSize);
            tcpCrl->dataSize = htonl(0); 
            break;
        default:
            return false;
    }

    if(shoit_network_writen(sender->tcpSockfd, buffer, sizeof(shoit_control_t)+dataSize) != (sizeof(shoit_control_t)+dataSize))              
    {                                                                                                                                
        SHOIT_LOG("tcp send failed(%s).\n",__func__);                                                                     
        return false;
    }              

    return true;
}
