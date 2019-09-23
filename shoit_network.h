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
#ifndef SHOIT_NETWORK_H__
#define SHOIT_NETWORK_H__

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>

/*
*resolve address
*out sa, salen
*return : if success return 0 ,else return -1
*/
int shoit_network_resolve_address(struct sockaddr *sa, socklen_t *salen, const char *host, const int port, int family, int type,int proto);


/*
*if noblocking is 0, set blocking,else set noblock
*if return -1 ,set failed
*/                                                                                                                                    
int shoit_network_set_noblocking(int fd,int noblocking) ;

/*
*set and show udp socket buffer size
*if sockBufsize >0 will be set
*if verbose > 1 will be show current value
*/
void shoit_network_set_socket_buffer(int udpSockfd, int sockbufsize, int verbose );

/*
*UDP socket, bind local address and connect remote
*parameter:
*   udpLocalAddr will be full and return
*return
*   return udp socket fd or -1
*/
int shoit_network_udp_server_init(struct sockaddr_in *udpLocalAddr,int udpLocalPort,char *remoteHost,int remoteUdpPort);

/*
*as udp client,connect udp server
*Fill in the structure whith the address of the server: udpServerAddr 
*return udp socket fd ,else retun -1
*/

int shoit_network_connect_udp(struct sockaddr_in *udpServerAddr,int localPort,char *remoteHost,int remoteHostPort);


/*
*connect remote tcp,timeout is 5 second
*return tcpsocket handle
*/
int shoit_network_connect_tcp(char * remoteHost,int remoteHostPort);


/*
*Note: this server ONLY accept one client and close listenfd
*input paramater clientAddr will be full with client address, maybe later will be use,eg: check correct client connected
*if  input paramater clientAddr is NULL, i will ignore it.
*return connefd ,error return -1;
*/
int shoit_network_tcp_server_init(char *serverHost,int serverPort,struct sockaddr_in *clientAddr);


/*
A read or write on a stream socket might input or output fewer bytes than requested, but this is not an error condition. 
The reason is that buffer limits might be reached for the socket in the kernel. All that is required to input or output 
the remaining bytes is for the caller to invoke the read or write function again. Some versions of Unix also exhibit this 
behavior when writing more than 4,096 bytes to a pipe. This scenario is always a possibility on a stream socket with read, 
but is normally seen with write only if the socket is nonblocking. Nevertheless, we always call our writen function instead 
of write, in case the implementation returns a short count.

*always read nbytes data or blocking until read return 0 or error(-1).
*blocking: return -1 or 0 or nbytes bytes 
*Note: Don't use in noblocking model
*/
ssize_t shoit_network_readn(int fd, void *vptr,ssize_t nbytes);
ssize_t shoit_network_writen(int fd,void *vptr,ssize_t nbytes);

    

#endif
