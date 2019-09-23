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
#ifndef SHOIT_SENDER_H__
#define SHOIT_SENDER_H__

#include "shoit.h"
#include <signal.h>
#include <pthread.h>


//public
void sender_run_loop(shoit_core_t *sender,uint32_t sendRate,uint32_t packetSize,int fromFd);


/////static functing//

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

#endif
