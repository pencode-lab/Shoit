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
#include "shoit.h"
#include "shoit_network.h"
#include "shoit_misc.h"
#include "shoit_receiver.h"

static void catch_signal(int sig)
{
    if(sig == SIGBUS){
        fprintf(stderr,"Local disk space is full,Could not complete transfer!!\n");
        exit(1);
    }
}

static void helpe(const char *progname)
{
    printf("Usage: %s [remote host] [password] [options] \n"
           "Options:\n"
           "  -f <fileName> Specifies the save file name(default: xxxx.shoit.recv.time)\n"
           "  -p <number>   Specifies the remote(sender) port number(default: 38000)\n"
           "  -v            Show version and copyight infomation\n"
           "  -E <log_file> print log infomation to file,(default:print to stderr)\n"
           "  -d            Run as daemon\n"
           "  -T            Run as test\n"
           "  -h            prints this help\n"
           "\n",
           progname);
    exit(0);
}

int main(int argc,char **argv)
{


    char *password=NULL;
    char *remoteHost=NULL;
    int port = 38000;
    char *saveFileName=NULL;//[SIZEOFFILENAME]={0};

    struct sockaddr_storage sa;
    socklen_t salen;
    char *logFile=NULL;

    bool needDaemon=false;

    /* resolve command line options and arguments */
    char ch;
    while ((ch = getopt(argc, argv, "f:p:E:vdTh")) != -1) {
        switch (ch) {                                                                                                                   
            case 'f':                                                                                                                   
                saveFileName =shoit_misc_nocr(strdup(optarg));
                break;                                                                                                                  
            case 'p':                                                                                                                   
                port = atoi(optarg)>0 ? atoi(optarg):port;                                                                              
                break;                                                                                                                  
            case 'E':
                logFile=shoit_misc_nocr(strdup(optarg));
                break;
            case 'v':                                                                                                                   
                shoit_show_copyright();                                                                                               
                exit(1);                                                                                                                
            case 'd':                                                                                                                
                needDaemon=true;                                                                                                     
                if(logFile==NULL) logFile="./shoit_recv.log";
                break; 
            case 'h':                                                                                                                   
            default:                                                                                                                    
                helpe(argv[0]);                                                                                                         
                exit(1);                                                                                                                
        }                                                                                                                               
    }//while   
    if (argc < 3)
    {   
        helpe(argv[0]);
        exit (1);
    }
 
    signal(SIGBUS,catch_signal);

    argc -= optind;                                                                                                                     
    argv += optind;                                                                                                                     
    if (argc != 0){                                                                                                                     
        remoteHost  = *argv++;
        password    = *argv++;
    }

    if(!remoteHost || !password) {
        helpe(argv[0]);
        exit(1);
    }
    remoteHost = shoit_misc_nocr(remoteHost);
    password = shoit_misc_nocr(password);

    
    if (shoit_network_resolve_address((struct sockaddr *)&sa, &salen, remoteHost, port, AF_INET, SOCK_DGRAM, 0) != 0){
        fprintf(stderr,"remote host<%s> is unknown, can't resolve...\n",remoteHost);
        exit(1);
    }


    if(needDaemon)
        shoit_misc_daemon("./");

     //init sender                                                                                                                    
    shoit_core_t receiver;
    bzero(&receiver,sizeof(receiver));
    receiver.verbose=1;
    receiver.password = password;

    memcpy(receiver.remoteHost,remoteHost,sizeof(receiver.remoteHost)-1);
    receiver.tcpPort = port;

    /*bind local udp server*/
    receiver.udpLocalPort = port; 
    receiver.udpRemotePort = port+1;
    receiver.bindHost =NULL;
    receiver.runing = false;

    FILE *log = shoit_misc_open_log(logFile);
    receiver.log = (log!=NULL) ? log:stderr;/*if open logfile error,set default is stderr*/

    receiver.iStream = false;
    int toFd =-1;
    receiver_run_loop(&receiver,saveFileName,toFd);  
    free(saveFileName);


    exit(0);
    
}
