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
#include "shoit_sender.h"


static void helpe(const char *progname)
{
    printf("Usage: %s [send file] [password] [options]\n"
           "Options:\n"
           "  -w <Kbps>         specifies the bind width(default 100Mbits: 100000)\n"
           "  -l <Bytes>        specifies the packet size (default: MTU)\n"
           "  -s <bind server>  specifies the bind ip or host (default:0.0.0.0)\n"
           "  -p <number>       specifies the listen port number (default: 38000)\n"
           "  -v                Show version and copyight infomation\n"
           "  -d                Run as daemon\n"
           "  -T                Run as test\n"
           "  -E <log_file>     print log infomation to file,(default:print to stderr)\n"
           "  -h                prints this help\n"
           "\n",
           progname);
    exit(0);
}


static bool open_data_source(shoit_core_t *sender)
{

    sender->fromFd = -1;
    if(!sender->iStream){/*data source is file of disk*/

        if(!sender->fileName || access(sender->fileName,F_OK)==-1){
            fprintf(stderr,"The file does not exist <%s>\n",sender->fileName);
            return false;
        }

        struct stat filestat;
        if (stat(sender->fileName, &filestat) < 0) {
            fprintf(stderr,"stat error(%s).\n",strerror(errno));
            return false;
        }
        sender->fileSize = filestat.st_size;
        sender->fromFd = open(sender->fileName, O_RDONLY);
        if (sender->fromFd < 0) {
            fprintf(stderr,"open file failed(%s).\n",sender->fileName);
            return false;
        }
    }
}

static void close_data_source(shoit_core_t *sender)
{
    if(sender->fromFd !=-1) {
        close(sender->fromFd);
        sender->fromFd = -1;
    }
}

int main(int argc,char **argv)
{
    int sendRate = SEND_RATE;
    int packetSize = MTU; 
    int port = 38000;
    bool isTest = false;

    char *logFile =NULL;

    bool useConf = true;

    char *localhost=NULL;
    char *password; 
    char *sendFile;

    bool needDaemon=false;

    struct sockaddr_storage sa;
    socklen_t salen;

    static shoit_callbacks_t dataSource_callbacks = {&open_data_source,&close_data_source,NULL};

    /* resolve command line options and arguments */
    char ch;
    while ((ch = getopt(argc, argv, "w:l:s:p:E:vdTh")) != -1) {
        switch (ch) {
            case 'w': 
                sendRate = atoi(optarg)>0 ? atoi(optarg) : sendRate;
                useConf = false;
                break;
            case 'l':
                packetSize = (atoi(optarg)>512 && (atoi(optarg) < 65535)) ? atoi(optarg):packetSize;
                useConf = false;
                break;
            case 's':
                localhost = strdup(optarg);
                break;
            case 'p':
                port = atoi(optarg)>0 ? atoi(optarg):port;
                break;
            case 'T':
                isTest=true;
                break;
            case 'E':/* event logging */
                logFile = strdup(optarg); 
                break;
            case 'd':
                needDaemon=true;
                if(logFile==NULL) logFile="./shoit_sender.log";                                                                    
                break;       
            case 'v':
                shoit_show_copyright();
                exit(1);
            case 'h':
            default:
                helpe(argv[0]);
                exit(1); 
        }
    }//while 

    if(argc <3){                                                                                                                     
        helpe(argv[0]);                                                                                                              
        exit(0);                                                                                                                     
    }    

    argc -= optind;
    argv += optind;
    if (argc != 0){
        sendFile = *argv++;
        password = *argv++;

    }

    password = shoit_misc_nocr(password);
    sendFile = shoit_misc_nocr(sendFile);
    if(!password || !sendFile) {
        helpe(argv[0]);
        exit(1);
    }

    /*daemon*/
    if(needDaemon)
        shoit_misc_daemon("./");

    //init sender
    shoit_core_t sender;
    bzero(&sender,sizeof(sender));

    sender.verbose= 1;
    sender.fileName = sendFile;

    sender.bindHost = localhost;
    sender.password = password; 

    /*bind local port,remote udpserver port will be control channel deliver from receiver*/
    sender.tcpPort = port;

    
    FILE *log = shoit_misc_open_log(logFile);
    sender.log = (log!=NULL) ? log:stderr;/*if open logfile error,set default is stderr*/ 


    sender.iStream =false; 

    sender.callbacks = &dataSource_callbacks;

    /*run sender*/
    sender_run_loop(&sender,sendRate,packetSize);


    
    exit(0);
}
