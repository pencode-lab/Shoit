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
#ifndef SHOIT_MISC_H__
#define SHOIT_MISC_H__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <limits.h>
#include <pthread.h>


/*
*Functionality:
*    return substring of s:end at the first occurrence of the character c ('','\n','\n').
*Parameters:
*    string s.
*Returned value:
*   substring.
*/

char *shoit_misc_nocr(char *s);

/*
*Functionality:
*    check the current time, 64bit, in microseconds.
*Parameters:
*    None.
*Returned value:
*   current time in microseconds.
*/
uint64_t shoit_misc_get_timer();

/*
*Functionality:
*   Seelp until CC "nexttime".
*Parameters:
*   [in] nexttime: next time the caller is waken up.
*Returned value:
*   None.
*/
void shoit_misc_sleepto(uint64_t nexttime, pthread_mutex_t *tickLock,pthread_cond_t *tickCond);

/*
*input line: 10.0.0.2:443
*output : host ="10.0.0.2", port = 22
*if success return 0, else return -1
*/
int shoit_misc_resolve_line(char *line,char *host,int *port);


char *shoit_misc_from_path_get_filename(char *path);

/*
*log
*/

FILE *shoit_misc_open_log(char *logFile);
void shoit_misc_log(FILE *fp,char *format, ...);
void shoit_misc_close_log(FILE *fp);


/*
*htonl or ntohl only uint32, i need uint64
*/
uint64_t shoit_misc_htonll(uint64_t lll);
uint64_t shoit_misc_ntohll(uint64_t nll);

/*
*return from start to now usecs
*/
int shoit_misc_report_time(struct timeval *start);


void shoit_misc_daemon(char *work_dir);


#endif
