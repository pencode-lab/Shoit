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
#include "shoit_receiver.h"

#include "shoit_misc.h"
#include "shoit_network.h"
#include "shoit_progress.h"

#define SHOIT_LOG(format, ...) do{ shoit_misc_log(receiver->log,format,##__VA_ARGS__); } while(0)

static progress_t progress;


////////////////////static func/////////////
static bool echo_to_sender_on_tcp(shoit_core_t *receiver,int status);
static void _receiver_run_loop(shoit_core_t *receiver,char *fileName);                                                               
static void receiver_free(shoit_core_t *receiver);
static void blast_udp_recving(shoit_core_t *receiver);
static bool prepare_and_receiver_init(shoit_core_t *receiver,char *srcFileName,char *saveFileName);
static bool each_round_transfer_init(shoit_core_t *receiver);

static void transfer_thread_run(void *argv);
static void progress_thread_run(void *argv);

//////////////////////////////////////////////

void receiver_run_loop(shoit_core_t *receiver,char *recvFile,int toFd)
{
    do{
        /*init for thread syn*/
        shoit_init_synch(receiver);
        receiver->runing = false;
        receiver->bucket=NULL;
        memset(&progress,0,sizeof(progress));

        if(receiver->iStream) receiver->toFd = toFd;
    
        
        /*connect sender's tcp server*/
        receiver->tcpSockfd =shoit_network_connect_tcp(receiver->remoteHost,receiver->tcpPort);
        if(receiver->tcpSockfd==-1){
            SHOIT_LOG("connect remote tcp server failed.\n");        
            break;
        }


        receiver->runing = true; 
        /*create pthread*/
        //create data transfer thread base on udp
        pthread_t transfer_thid;
        pthread_create(&transfer_thid,NULL,(void*)transfer_thread_run,(void*)receiver);
        
        //create progress thread
        pthread_t prog_thid;
        pthread_create(&prog_thid,NULL,(void*)progress_thread_run,(void*)receiver);




        /*run tcp control loop*/
        _receiver_run_loop(receiver,recvFile);



        /*wait thread exit*/
        sem_post(&receiver->dataSem);
        sem_post(&receiver->progSem); 
        pthread_join(transfer_thid,NULL); 
        pthread_join(prog_thid,NULL); 

    }while(0);

    receiver_free(receiver);

    return;
}


////////static function/////////

static void progress_thread_run(void *argv)
{
    shoit_core_t *receiver = (shoit_core_t*)argv; 
    int done=0;
    int prog=0;
    struct timeval curTime, startTime;

    sem_wait(&receiver->progSem);
    if(!receiver->runing) pthread_exit(0);

    shoit_progress_init(&progress, "", 100, PROGRESS_NUM_STYLE);
    if(receiver->iStream){
        SHOIT_LOG("Mbit/s      Recv(Kbytes)  time(s)\n") ;
        gettimeofday(&startTime, NULL);
        while(!done && receiver->runing){
            gettimeofday(&curTime, NULL);
            progress.utime = (curTime.tv_sec - startTime.tv_sec) + 1e-6*(curTime.tv_usec - startTime.tv_usec);
            shoit_progress_for_stream(receiver->log,&progress,0);
            usleep(1000000);
        }                                                                                                                            
                                                                                                                                     
    }else{    

        SHOIT_LOG("Mbit/s      recv/total(Kbytes)  time(s)     progress\n") ;
        gettimeofday(&startTime, NULL);
        while(!done && receiver->runing){
            prog = (float) progress.total / (float) receiver->fileSize * 100;
            gettimeofday(&curTime, NULL);
            progress.utime = (curTime.tv_sec - startTime.tv_sec) +1e-6*(curTime.tv_usec - startTime.tv_usec);
            shoit_progress_show_all(receiver->log,&progress,(int)prog/100.0f,(long)(receiver->fileSize >>10));
            usleep(1000000);
        }
    }

    gettimeofday(&curTime, NULL);
    SHOIT_LOG("\n");
    {//show complete info

        float dt = (curTime.tv_sec - startTime.tv_sec) + 1e-6*(curTime.tv_usec - startTime.tv_usec-1000000);
        uint64_t got =progress.total; 
        float mbps = 1e-6 * 8 * got / (dt==0 ? .01 : dt);                                                                        

        if(receiver->iStream)
            SHOIT_LOG("\nComplete transfer %dK in %.2fs (%g Mbit/s)",(long)(got>>10), dt, mbps);
        else
            SHOIT_LOG("\nComplete transfer %d/%dK in %.2fs (%g Mbit/s)",(long)(got>>10), (long)(receiver->bucketSize>>10), dt, mbps);
    }
    shoit_progress_destroy(&progress);
    pthread_exit(0);
}

/*
*receive UDP packet
*/
static void blast_udp_recving(shoit_core_t *receiver)
{
    int done, actualPayloadSize, retval;
    uint64_t seqno;
    struct timeval start;

    char *msg = (char *) malloc(receiver->packetSize);    
    shoit_udp_hdr *udphdr = (shoit_udp_hdr*)msg;

    struct timeval timeout;
    fd_set rset,orgSet;
    int maxfdpl;
    done = 0; seqno = 0;

    #define QMAX(x, y) ((x)>(y)?(x):(y))
    
    maxfdpl = QMAX(receiver->udpSockfd,receiver->pipe[0]) + 1;
    FD_ZERO(&orgSet);
    FD_SET(receiver->udpSockfd, &orgSet);
    FD_SET(receiver->pipe[0], &orgSet);
    gettimeofday(&start, NULL);


    while (!done && receiver->runing)
    {
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        memcpy(&rset,&orgSet,sizeof(orgSet));
        retval = select(maxfdpl, &rset, NULL, NULL, &timeout);
        if (retval == -1){
            perror("select()");
            break;
        }else if(retval>0){
            if (FD_ISSET(receiver->udpSockfd, &rset)){
                int nread = recv(receiver->udpSockfd, msg, receiver->packetSize, 0);
                if(nread>0){
                    seqno = udphdr->seqno;
                    actualPayloadSize = ntohl(udphdr->payloadSize);
                    uint64_t offset= (uint64_t)(seqno*receiver->payloadSize);
                    /*
                    if((offset+actualPayloadSize) >= 1426063360) 
                        fprintf(stderr,"bucket at:0x%p, offset:%ld, mapSize:%ld, offfset+payload:%ld\n",receiver->bucket,
                            offset,receiver->mapSize,actualPayloadSize+offset);
                    */
                    memcpy((char *)receiver->bucket+offset,udphdr->payload,actualPayloadSize);
                    shoit_update_error_bitmap(receiver,seqno);
                    receiver->receivedNumberOfPackets ++;
                    progress.total += nread;
                }else {
                    perror("recv udp packet");
                    done =1;
                }
            }else if(FD_ISSET(receiver->pipe[0],&rset)){
                char c;
                if(read(receiver->pipe[0],&c,1)<0) perror("read pipe");
                done = 1;                                                                                                                
            }
        }

    }       
    free(msg);
    return;
}


static bool transfer_data_round_circle(shoit_core_t *receiver)
{
    int done = 0;
    bool needUpdate = true;
    struct timeval curTime, startTime;
    uint32_t  sendDataSize =0;

    /*init for this circle*/
    sem_wait(&receiver->dataSem);
    bool okInit = each_round_transfer_init(receiver);
    if(okInit){
        echo_to_sender_on_tcp(receiver,READY);
    }else {
        if(receiver->verbose>1)
            SHOIT_LOG("round_transfer_init failed:have't more data\n");
        return false;
    }

    shoit_control_t *tcpCrl=(shoit_control_t *)receiver->errorBitmap;
    bzero(tcpCrl,sizeof(shoit_control_t));                                                                                           
    tcpCrl->status = BITMAP;     
    while(okInit && !done && receiver->runing){                                                                                                
        gettimeofday(&startTime, NULL);                                                                                              
        blast_udp_recving(receiver);                                                                                                 
        gettimeofday(&curTime, NULL);                                                                                                
                                                                                                                                     
        /*update and return the count of errors*/                                                                                    
        uint64_t errnum=0;                                                                                                           
        if ((errnum=shoit_update_hash_table(receiver,needUpdate))== 0){/*all receiver,error =0*/                                     
            done = 1;                                                                                                                
            sendDataSize =0;                                                                                                         
        }else  
            sendDataSize = receiver->sizeofErrorBitmap;                                                                           

        tcpCrl->dataSize = htonl(sendDataSize);                                                                                      
                                                                                                                                     
        // Send back Error bitmap                                                                                                    
        int n = shoit_network_writen(receiver->tcpSockfd,receiver->errorBitmap,sizeof(shoit_control_t)+sendDataSize);                
        if(n!=sendDataSize+sizeof(shoit_control_t)){                                                                                 
            SHOIT_LOG("write bitmap to sender faild.(%s)\n",strerror(errno));                                                        
            done =1;                                                                                                                 
        }                                                                                                                            
        if(errnum==0 && receiver->iStream) {                                                                                          
            uint64_t totalSize = receiver->payloadSize*(receiver->totalNumberOfPackets-1) +receiver->lastPayloadSize; 
            if(receiver->callbacks->on_recv_data(receiver,receiver->bucket,totalSize)<0){
                SHOIT_LOG("write to receiver's toFd error:%s\n",strerror(errno));                                        
                break;                                                                                                   
            }                                                                                                            
            
        }
    }           

    if(receiver->useMmap && receiver->mapSize >0){
        munmap(receiver->bucket,receiver->mapSize);
    }

    return true;
}

static void transfer_thread_run(void *argv)
{
    shoit_core_t *receiver = (shoit_core_t *)argv;

    if(receiver->runing){
        //create udpserver
        receiver->udpRemotePort = receiver->udpLocalPort + 1;
        receiver->udpSockfd=shoit_network_udp_server_init(&receiver->udpServerAddr,
                                                            receiver->udpLocalPort,
                                                            receiver->remoteHost,
                                                            receiver->udpRemotePort);
        if(receiver->verbose>1) SHOIT_LOG("start udpserver 0.0.0.0:%d <-> %s:%d",
                            receiver->udpLocalPort,receiver->remoteHost,receiver->udpRemotePort);

        if(receiver->udpSockfd==-1){
            receiver->runing=false;
            SHOIT_LOG("start udp server failed.\n");
        }
    }

    while(receiver->runing){
        transfer_data_round_circle(receiver);
    }
    receiver->runing=false; 
    pthread_exit(0);
}

static bool echo_to_sender_on_tcp(shoit_core_t *receiver, int type)
{
    char buffer[MTU]={0};
    int dataSize =0;
    shoit_control_t *tcpCrl=(shoit_control_t *)buffer;

    memset(buffer,0,sizeof(buffer));
    tcpCrl->status =type;
    switch(type){
        case CONNECT:
        {
            dataSize= strlen(receiver->password);
            tcpCrl->dataSize =htonl(dataSize);
            memcpy(tcpCrl->data,receiver->password,dataSize);
        }break;
        case READY:
        {
            dataSize=0;
            tcpCrl->dataSize = htonl(0);
        }break;
        default:
            return false;                                                                                                            
    }                                                                                                                                
                                                                                                                                     
    if(shoit_network_writen(receiver->tcpSockfd, buffer, sizeof(shoit_control_t)+dataSize) != (sizeof(shoit_control_t)+dataSize))                    
    {                                                                                                                                
        SHOIT_LOG("tcp send failed(%s).\n",__func__);                                                                                
        return false;                                                                                                                
    }                                                                                                                                
                                                                                                                                     
    if(receiver->verbose>1) SHOIT_LOG("echo_to_sender_on_tcp ok....:%d\n",type);
    return true;                                                                                                                     
}    


static void _receiver_run_loop(shoit_core_t *receiver,char *saveFile)
{
    struct timeval timeout;
    fd_set rset,orgSet;
    int maxfdpl;
    int done=0;
    int retval;


    shoit_control_t tcpCrl;

    maxfdpl = receiver->tcpSockfd +1;
    FD_ZERO(&orgSet);
    FD_SET(receiver->tcpSockfd, &orgSet);

    if(!echo_to_sender_on_tcp(receiver,CONNECT)){
        SHOIT_LOG("send CONNECT SIGNAL to server failed.\n");
        return;
    }

    while(!done && receiver->runing){
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        memcpy(&rset,&orgSet,sizeof(orgSet));
        retval = select(maxfdpl, &rset, NULL, NULL, &timeout);

        if(retval<0){/*error*/
            SHOIT_LOG("select return -1 (%s) at 0xe01\n",strerror(errno));
            done =1;
        }
        if(retval>0 && FD_ISSET(receiver->tcpSockfd, &rset)){
            if(shoit_network_readn(receiver->tcpSockfd,(char*)&tcpCrl,sizeof(tcpCrl))==sizeof(shoit_control_t)){

                if(receiver->verbose>1) SHOIT_LOG("from client get status signal %d\n",tcpCrl.status);
                switch(tcpCrl.status){
                    case ROUND:/*sender echo request*/
                    {
                        uint64_t fileSize = shoit_misc_ntohll(tcpCrl.fileSize);
                        uint64_t buketSize = shoit_misc_ntohll(tcpCrl.bucketSize); 
                        uint32_t payloadSize = ntohl(tcpCrl.payloadSize);
                        uint32_t dataSize = ntohl(tcpCrl.dataSize);
                        char filename[SIZEOFFILENAME]={0};

                        if(shoit_network_readn(receiver->tcpSockfd,filename,dataSize)!=dataSize){
                            SHOIT_LOG("read filename data failed.\n");
                            done = 1;
                            break;
                        }
                        receiver->fileSize = fileSize;
                        receiver->bucketSize = buketSize;
                        receiver->payloadSize = payloadSize;                                                                         
                        receiver->packetSize =payloadSize+sizeof(shoit_udp_hdr);                                                  


                        if(!prepare_and_receiver_init(receiver,filename,saveFile)){
                            done =1;
                            break;
                        }

                        /*notic thread start udp server*/
                        if(receiver->verbose>1) SHOIT_LOG("recv ROUND signal and wakeup sem\n");
                        sem_post(&receiver->dataSem);/*wakeup a cycle recv*/
                        sem_post(&receiver->progSem);
                    }break;
                    case CLOSE: /*complete,sender will close*/
                        sem_post(&receiver->dataSem);
                        done =1;
                        break;
                    case HEART:
                        if(receiver->verbose>1) SHOIT_LOG("recv HEART signal\n");
                        break;
                    case UDPEND:
                        if(write(receiver->pipe[1]," ",1)<0) perror("write pipe"); /*use pipe notice thread*/
                        break;
                    default:
                        SHOIT_LOG("read error tcpcrl,ignore the recv data. 0xe03\n");
                        done =1;
                        break;
                    
                }
            }else{
                if(write(receiver->pipe[1]," ",1)<0) perror("write pipe"); /*use pipe notice thread*/
                SHOIT_LOG("remote socket close.\n");                                                                                 
                done =1;  
            }
        }
        
        
    }
    receiver->runing = false;
    return;
}

static bool each_round_transfer_init(shoit_core_t *receiver)
{
    if(receiver->runing ==  false) return false;//CLOSE sig
    if (receiver->bucketSize % receiver->payloadSize == 0)                                                                           
    {                                                                                                                                
        receiver->totalNumberOfPackets = receiver->bucketSize/receiver->payloadSize;                                                 
        receiver->lastPayloadSize = receiver->payloadSize;                                                                           
    }                                                                                                                                
    else                                                                                                                             
    {                                                                                                                                
        receiver->totalNumberOfPackets = receiver->bucketSize/receiver->payloadSize + 1; /* the last packet is not full */           
        receiver->lastPayloadSize = receiver->bucketSize - receiver->payloadSize * (receiver->totalNumberOfPackets - 1);             
    }                                                                                                                                
                                                                                                                                     
    receiver->receivedNumberOfPackets = 0;                                                                                           
    receiver->remainNumberOfPackets = receiver->totalNumberOfPackets;                                                                
    receiver->sizeofErrorBitmap = receiver->totalNumberOfPackets/8 + 2;                                                              
    receiver->isFirstBlast = true;                                                                                                   
    /* Initialize the hash table */                                                                                                  
    for (uint64_t i=0; i<receiver->totalNumberOfPackets; i++)                                                                        
    {                                                                                                                                
        receiver->hashTable[i] = i;                                                                                                  
    }                                                                                                                                
    shoit_init_error_bitmap(receiver); 

    return true;
    
}


static bool prepare_and_receiver_init(shoit_core_t *receiver,char *srcFileName,char *saveFileName)
{

    //if(receiver->bucket!=NULL) return true; /*already malloc and init*/

    if(receiver->iStream==false){/*use mmap*/

        if(receiver->bucket == NULL) {//first come to here

            char destFile[512]={0};
            bzero(destFile,sizeof(destFile));
            if(saveFileName==NULL)
                snprintf(destFile,sizeof(destFile)-1,"%s.shoit.recv.%ld",srcFileName,time(NULL));
            else
                memcpy(destFile,saveFileName,strlen(saveFileName));

            if(receiver->verbose>0)
                SHOIT_LOG("remote send file[%s], will save as[%s] to local disk.\n",srcFileName,destFile);

            receiver->toFd = open(destFile, O_RDWR|O_CREAT|O_TRUNC, 0666);
            if(receiver->toFd <0 || ftruncate(receiver->toFd,receiver->fileSize)){ 
                SHOIT_LOG("open file error(%s)\n",strerror(errno)); 
                return false;
            }
            receiver->useMmap = true;
            receiver->leftSize = receiver->fileSize;

            //fprintf(stderr,"init map,fileSize:%ld,leftSize:%ld \n",receiver->fileSize,receiver->leftSize);

        }
   
        if(receiver->leftSize <=0)  return false;

        size_t mapSize = (receiver->leftSize/receiver->bucketSize > 0) ? receiver->bucketSize: receiver->leftSize;
        off_t offset= receiver->fileSize - receiver->leftSize;//receiver->bucketSize *((receiver->fileSize - receiver->leftSize)/receiver->bucketSize);
        
        receiver->bucket= (char *)mmap(NULL, mapSize, PROT_READ|PROT_WRITE, MAP_SHARED,receiver->toFd,offset);
        if (receiver->bucket== MAP_FAILED) {
            //fprintf(stderr,"mapSize = %ld,bucketSize:%ld offset = %ld\n",mapSize,receiver->bucketSize,offset); 
            SHOIT_LOG("mmap error(%s)\n",strerror(errno));
            return false;
        }    
        receiver->bucketSize = mapSize;
        receiver->leftSize -= mapSize;
        receiver->mapSize = mapSize;
        //fprintf(stderr,"mapSize = %ld,bucketSize:%ld offset = %ld\n",mapSize,receiver->bucketSize,offset); 

    }else{
        if(receiver->bucket!=NULL) return true; /*already malloc and init*/
        receiver->bucket =(char*)malloc(receiver->bucketSize);
        receiver->useMmap = false;                                                                                                    
        if(receiver->bucket == NULL) {
            SHOIT_LOG("malloc error(%s)\n",strerror(errno)); 
            return false;
        }
    }

   
    if (receiver->bucketSize % receiver->payloadSize == 0)
    {
        receiver->totalNumberOfPackets = receiver->bucketSize/receiver->payloadSize;
        receiver->lastPayloadSize = receiver->payloadSize;
    }
    else
    {
        receiver->totalNumberOfPackets = receiver->bucketSize/receiver->payloadSize + 1; /* the last packet is not full */
        receiver->lastPayloadSize = receiver->bucketSize - receiver->payloadSize * (receiver->totalNumberOfPackets - 1);
    }

    receiver->receivedNumberOfPackets = 0;
    receiver->remainNumberOfPackets = receiver->totalNumberOfPackets;
    receiver->sizeofErrorBitmap = receiver->totalNumberOfPackets/8 + 2;
    receiver->errorBitmap = (char *)malloc(receiver->sizeofErrorBitmap+sizeof(shoit_control_t));
    receiver->hashTable = (uint64_t *)malloc(receiver->totalNumberOfPackets * sizeof(uint64_t));
    receiver->isFirstBlast = true;
    
    if(receiver->verbose>1) SHOIT_LOG("totalNumberOfPackets: %d", receiver->totalNumberOfPackets);
    if (receiver->errorBitmap == NULL)
    {
        SHOIT_LOG("malloc %ld bytes error(%s)\n",receiver->sizeofErrorBitmap,strerror(errno));
        return false;
    }
    if (receiver->hashTable == NULL)
    {
        SHOIT_LOG("malloc hashTable failed\n");
        return false;
    }

    /* Initialize the hash table */
    for (uint64_t i=0; i<receiver->totalNumberOfPackets; i++)
    {
        receiver->hashTable[i] = i;
    }
    shoit_init_error_bitmap(receiver);

    return true;
}


static void receiver_free(shoit_core_t *receiver)
{
    if(receiver->bucket !=NULL){
        if(receiver->useMmap){
            ;//munmap(receiver->bucket, receiver->bucketSize);
        }else{
            free(receiver->bucket);
        }
        receiver->bucket=NULL;
    }

    if(receiver->fromFd !=-1) {
        close(receiver->fromFd);
        receiver->fromFd = -1;
    }
    if(receiver->toFd !=-1){
        close(receiver->toFd);
        receiver->toFd =-1;
    }

    if(receiver->bindHost!=NULL) {
        free(receiver->bindHost);
        receiver->bindHost=NULL;
    }
    if(receiver->errorBitmap!=NULL){
        free(receiver->errorBitmap);
        receiver->errorBitmap = NULL;
    }

    if(receiver->hashTable!=NULL){
        free(receiver->hashTable);
        receiver->hashTable = NULL;
    }

    shoit_destory_synch(receiver);
    //last free log                                                                                                                  
    SHOIT_LOG("free sender complete.\n");                                                                                            
    shoit_misc_close_log(receiver->log);     
}
