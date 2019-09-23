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
#include "shoit_network.h"


/*
*resolve address
*out sa, salen
*return : if success return 0 ,else return -1
*/
int shoit_network_resolve_address(struct sockaddr *sa, socklen_t *salen, const char *host, const int port, int family, int type,int proto)
{
    struct addrinfo hints, *res;
    int err;
    char sport[8]={0};

    memset(sport,0,8);
    snprintf(sport,7,"%d",port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = proto;
    hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV | AI_PASSIVE;
    if ((err = getaddrinfo(host, sport, &hints, &res)) != 0 || res == NULL) {
        fprintf(stderr, "failed to resolve address:%s:%s:%s\n", host, sport,
                err != 0 ? gai_strerror(err) : "getaddrinfo returned NULL");
        return -1;
    }
    memcpy(sa, res->ai_addr, res->ai_addrlen);
    *salen = res->ai_addrlen;                                                                                                        
    freeaddrinfo(res);                                                                                                               
    return 0;                                                                                                                        
}                                                                                                                                    
                                                                                                                                     
/*
*if noblocking is 0, set blocking,else set noblock
*if return -1 ,set failed
*/                                                                                                                                     
int shoit_network_set_noblocking(int fd,int noblocking) 
{                                                                                                                                    
    int flags = fcntl(fd, F_GETFL, 0);                                                                                               
    if(flags==-1) return -1;                                                                                                         

    if(noblocking) //set noblock
        return fcntl(fd, F_SETFL, flags|O_NONBLOCK);                                                                                     
    else
        return fcntl(fd, F_SETFL, flags&~O_NONBLOCK); 
                                                                                                                                     
}    


void shoit_network_set_socket_buffer(int udpSockfd, int sockbufsize, int verbose )
{
    int oldsend = -1, oldrecv = -1;
    int newsend = -1, newrecv = -1;
    socklen_t olen = sizeof(int);

    if(getsockopt(udpSockfd,SOL_SOCKET,SO_SNDBUF,&oldsend,&olen) < 0)
        perror("getsockopt: SO_SNDBUF");

    if(getsockopt(udpSockfd,SOL_SOCKET,SO_RCVBUF,&oldrecv,&olen) < 0)
        perror("getsockopt: SO_RCVBUF");

    if(sockbufsize > 0) {
        if(setsockopt(udpSockfd,SOL_SOCKET,SO_SNDBUF,&sockbufsize,sizeof(sockbufsize)) < 0)
            perror("setsockopt: SO_SNDBUF");
        if(setsockopt(udpSockfd,SOL_SOCKET,SO_RCVBUF,&sockbufsize,sizeof(sockbufsize)) < 0)
            perror("setsockopt: SO_RCVBUF");
        if(getsockopt(udpSockfd,SOL_SOCKET,SO_SNDBUF,&newsend,&olen) < 0)
            perror("getsockopt: SO_SNDBUF");
        if(getsockopt(udpSockfd,SOL_SOCKET,SO_RCVBUF,&newrecv,&olen) < 0)
            perror("getsockopt: SO_RCVBUF");
    }
    if(verbose>1) 
        fprintf(stderr, "UDP sockbufsize was %d/%d now %d/%d (send/recv)\n",oldsend, oldrecv, newsend, newrecv);
}

/*
*as udp client,connect udp server
*Fill in the structure whith the address of the server: udpServerAddr 
*return udp socket fd ,else retun -1
*/

int shoit_network_connect_udp(struct sockaddr_in *udpServerAddr,int localPort,char *remoteHost,int remoteHostPort)
{
    static struct sockaddr_in udpClientAddr;
    struct hostent *phe;
    int udpSockfd;

    // Fill in the structure whith the address of the server that we want to send to
    // udpServerAddr is class global variable, will be used to send data

    bzero(udpServerAddr, sizeof(struct sockaddr_in));
    udpServerAddr->sin_family = AF_INET;

    if (phe = gethostbyname(remoteHost))
        memcpy(&udpServerAddr->sin_addr, phe->h_addr, phe->h_length);
    else if ((udpServerAddr->sin_addr.s_addr = inet_addr(remoteHost)) == INADDR_NONE)
    {   
        perror("can't get host entry");
        return -1;
    }
    udpServerAddr->sin_port = htons(remoteHostPort);

    /* Open a UDP socket */
    if ((udpSockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket error");
        return -1;
    }

    // Allow the port to be reused.
    int yes = 1;
    if (setsockopt(udpSockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
        perror("setsockopt");
        close(udpSockfd);
        return -1;
    } 

    /* Bind any local address for us */
    bzero(&udpClientAddr, sizeof(udpClientAddr));
    udpClientAddr.sin_family = AF_INET;
    udpClientAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    udpClientAddr.sin_port = htons(localPort);
    if((bind(udpSockfd, (struct sockaddr *)&udpClientAddr, sizeof(udpClientAddr))) < 0) {
        perror("UDP client bind error");
        close(udpSockfd);
        return -1;
    }

    return udpSockfd;
}

/*
*UDP socket as server
*Use connected UDP to receive only from a specific host and port
*parameter:
*   udpLocalAddr will be full and return
*return
*   return udp socket fd or -1
*/
int shoit_network_udp_server_init(struct sockaddr_in *udpLocalAddr,int udpLocalPort,char *remoteHost,int remoteUdpPort)
{
    struct sockaddr_in cliaddr;
    struct hostent *phe;
    int udpSockfd;
    

    // Create a UDP sink 
    if ((udpSockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket error");
        return udpSockfd;
    }
    bzero((char *)udpLocalAddr, sizeof(struct sockaddr_in));
    udpLocalAddr->sin_family = AF_INET;
    udpLocalAddr->sin_addr.s_addr = htonl(INADDR_ANY);
    udpLocalAddr->sin_port = htons(udpLocalPort);

    if((bind(udpSockfd, (struct sockaddr *)udpLocalAddr, sizeof(struct sockaddr_in))) < 0) {
        perror("UDP bind error");
        close(udpSockfd);
        return -1;
    }

    // Use connected UDP to receive only from a specific host and port.
    bzero(&cliaddr, sizeof(cliaddr));
    if (phe = gethostbyname(remoteHost))
        memcpy(&cliaddr.sin_addr, phe->h_addr, phe->h_length);
    else if ((cliaddr.sin_addr.s_addr = inet_addr(remoteHost)) == INADDR_NONE){
        perror("gethostbyname remotehost error");
        close(udpSockfd);
        return -1;
    }

    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(remoteUdpPort);

    if (connect(udpSockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0)
    {
        perror("connect() error");
        close(udpSockfd);
        return -1;
    }

    return udpSockfd;
}

/*
*connect remote tcp,timeout is 5 second
*return tcpsocket handle
*error return -1
*/

int shoit_network_connect_tcp(char * remoteHost,int remoteHostPort)
{
    #define USEC(st, fi) (((fi)->tv_sec-(st)->tv_sec)*1000000+((fi)->tv_usec-(st)->tv_usec))
    static struct sockaddr_in tcpServerAddr;
    int retval = 0;
    struct hostent *phe;
    struct timeval start, now;
    int tcpSockfd;

    /*Create a TCP connection */
    bzero((char *)&tcpServerAddr, sizeof(tcpServerAddr));
    tcpServerAddr.sin_family = AF_INET;

    if (phe = gethostbyname(remoteHost))
        memcpy(&tcpServerAddr.sin_addr, phe->h_addr, phe->h_length);
    else if ((tcpServerAddr.sin_addr.s_addr = inet_addr(remoteHost)) == INADDR_NONE)
    {
        perror("can't get host entry");
        return -1;
    }
    tcpServerAddr.sin_port = htons(remoteHostPort);
    if((tcpSockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket error");
        return -1;
    }
    gettimeofday(&start, NULL);
    do {
        retval = connect(tcpSockfd, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr));
        gettimeofday(&now, NULL);
        if(retval<0) break;
    } while ((retval < 0) &&  (USEC(&start, &now) < 5000000));
    tcpSockfd = !retval ? tcpSockfd:-1;
    return tcpSockfd;
}

/*
*Note: this server ONLY accept one client and close listenfd
*input paramater clientAddr will be full with client address, maybe later will be use,eg: check correct client connected
*if  input paramater clientAddr is NULL, i will ignore it.
*return connefd ,error return -1;
*/
int shoit_network_tcp_server_init(char *serverHost,int serverPort,struct sockaddr_in *clientAddr)
{
    struct sockaddr_in tcpServerAddr;
    int listenfd,socketfd;

    // Create TCP connection as a server in order to transmit the error list 
    // Open a TCP socket 
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0))<0) {
        perror("socket error");
        return(-1);
    }

    int yes=1;
    if (setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
        perror("setsockopt");
        return(-1);
    } 
    
    /* Bind our local address so that the client can send to us */
    bzero(&tcpServerAddr, sizeof(tcpServerAddr));
    tcpServerAddr.sin_family = AF_INET;

    if(serverHost==NULL || strlen(serverHost)==0){                                                                                                    
        tcpServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);                                                                           
    }else{                                                                                                                           
        struct hostent *phe;                                                                                                         
        if (phe = gethostbyname(serverHost))                                                                                 
            memcpy(&tcpServerAddr.sin_addr, phe->h_addr, phe->h_length);                                                             
        else if ((tcpServerAddr.sin_addr.s_addr = inet_addr(serverHost)) == INADDR_NONE){                                    
            fprintf(stderr,"bind localhost error:%s\n",strerror(errno));                                                             
            close(listenfd);
            return(-1);
        }                                                                                                                            
                                                                                                                                     
    }                                                                                                                                
    tcpServerAddr.sin_port = htons(serverPort);                                                                                

    if((bind(listenfd, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr))) < 0) {
        perror("bind error");
        close(listenfd);
        return(-1);
    }
    
    if((listen(listenfd, 5))<0) {
        perror("listen error");
        close(listenfd);
        return(-1);
    }

   
    struct sockaddr_in tcpClientAddr;
    socklen_t clilen;

    clilen = sizeof(tcpClientAddr);
    socketfd = accept(listenfd, (struct sockaddr *) &tcpClientAddr, &clilen);

    if (socketfd < 0)
    {
        perror("accept error");
        return -1;
    } 

    close(listenfd);

    if(clientAddr!=NULL){
        memcpy(clientAddr,&tcpClientAddr,sizeof(tcpClientAddr));
    }


    return socketfd;
}




/*
*always read nbytes data or block until read return 0.
*blocking: return -1 or 0 or nbytes bytes 
*Note: Don't use in noblocking model
*/
ssize_t shoit_network_readn(int fd, void *vptr,ssize_t nbytes)
{
    ssize_t nleft, nread;
    char *ptr;

    ptr =(char*)vptr;
    nleft = nbytes;

    while(nleft > 0)
    {
        if ( (nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;      /* and call read() again */
            else
                return (-1);
        } else if (nread == 0)
            break;              /* EOF */

        nleft -= nread;
        ptr += nread;
    }
    return (nbytes - nleft);
}


ssize_t shoit_network_writen(int fd,void *vptr,ssize_t nbytes)
{
    ssize_t nleft, nwritten;
    char *ptr;

    ptr = (char*)vptr;
    nleft = nbytes;
    while(nleft > 0)
    {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;   /* and call write() again */
            else
                return (-1);    /* error */
        }

        nleft -= nwritten;
        ptr += nwritten;
    }
    return (nbytes -nleft);
}



/////////////////Testing function//////////////////////////////////////////////


#ifdef TESTING

#include <signal.h>

void signal_action(int sig)
{
    if(sig==SIGPIPE){
        fprintf(stderr,"generating SIGPIPE....\n");
        fprintf(stderr,"will exit..\n");
    }
}
int main(int argc,char **argv)
{
    /*shoit_network_tcp_server_init(char *serverHost,int serverPort)*/

    signal(SIGPIPE,signal_action);

    int fd = shoit_network_tcp_server_init("127.0.0.1",38000,NULL);
    if(fd>0){
        shoit_network_set_noblocking(fd,1);
        while(1){
            /*
            fprintf(stderr,"TCP/IP program crashes when calling write()\n");
            fprintf(stderr,"will write..,but will return EPIPE error on a broken pipe\n");
            fprintf(stderr,"so If you ignore the SIGPIPE signal, will crash....\n");
            fprintf(stderr,"you can use send replace write or Catch the SIGPIPE\n");
            fprintf(stderr,"send(2) will then return -1 with errno set to EPIPE, rather than generating a fatal SIGPIPE. \n");
                 int n = write(fd,"hello\n",6);
                 //int n = send(fd,"hello\n",6,MSG_NOSIGNAL);
                 fprintf(stderr,"write...:%d\n",n);
            */

            int nwrite =shoit_network_writen(fd, "hello\n",6);
            fprintf(stderr,"writen =%d\n",nwrite);
            char buffer[16]={0};
            int nread = shoit_network_readn(fd,buffer,16);
                         
            fprintf(stderr,"readn[%d]{%s}\n",nread,buffer);

            sleep(2);
        }
    }
    fprintf(stderr,"------------------\n");


    /*int shoit_network_connect_tcp(char * remoteHost,int remoteHostPort)*/
    int retval = shoit_network_connect_tcp("dribbble.com",443);
    fprintf(stderr,"connect tcp return :%d\n",retval);

    exit(0);
}    
#endif
