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
#include "shoit_misc.h"


/*
*Functionality:
*    return substring of s:end at the first occurrence of the character c ('','\n','\n').
*Parameters:
*    string s.
*Returned value:
*   substring.
*/

char *shoit_misc_nocr(char *s)
{
    if(s==NULL) return s;
    char *pos = s;

    while(*pos){
        if(*pos==' ' || *pos == '\n' || *pos=='\r') break;
        pos++;
    }
    *pos='\0';

    return s;
}


/*
*Functionality:
*    check the current time, 64bit, in microseconds.
*Parameters:
*    None.
*Returned value:
*   current time in microseconds.
*/
uint64_t shoit_misc_get_timer()
{
    struct timeval  t;
    gettimeofday(&t, 0);
    return t.tv_sec * 1000000ULL + t.tv_usec;
}

/*
*Functionality:
*   Seelp until CC "nexttime".
*Parameters:
*   [in] nexttime: next time the caller is waken up.
*Returned value:
*   None.
*/
void shoit_misc_sleepto(uint64_t nexttime, pthread_mutex_t *tickLock,pthread_cond_t *tickCond)
{
    uint64_t schedTime = nexttime;
    uint64_t t = shoit_misc_get_timer();

    while (t < schedTime)
    {
        struct timeval now;
        struct timespec timeout;
        gettimeofday(&now, 0);

        if (now.tv_usec < 990000){
            timeout.tv_sec = now.tv_sec;
            timeout.tv_nsec = (now.tv_usec + 10000) * 1000;
        } else {
            timeout.tv_sec = now.tv_sec + 1;
            timeout.tv_nsec = (now.tv_usec + 10000 - 1000000) * 1000;
        }

        pthread_mutex_lock(tickLock);
        pthread_cond_timedwait(tickCond, tickLock,&timeout);
        pthread_mutex_unlock(tickLock);
        t = shoit_misc_get_timer();
    }
}



int shoit_misc_resolve_line(char *line,char *host,int *port)
{
    char *pos;
    if(line==NULL || strlen(line)==0) return -1;
    if( (pos =strchr(line,':'))==NULL) return -1;
    
    memcpy(host,line,(int)(pos-line));
    pos ++; //del ':'
    if(pos==NULL) return -1; /*port error*/

    char *endptr;
    errno = 0;    /* To distinguish success/failure after call */
    long val = strtol(pos, &endptr,10);
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
                   || (errno != 0 && val == 0)) {
        return -1;
    }
    if (endptr == pos) {
        fprintf(stderr, "No digits were found\n");
        return -1;
    }
   
    *port = (int)val; 

    return 0;
    
}

char *shoit_misc_from_path_get_filename(char *path)
{
    char *pos;
    if(!path) return path;
    if((pos=strrchr(path,'/'))==NULL) return path;
    return(++pos);
}


FILE *shoit_misc_open_log(char *logFile)
{
    if(!logFile) return NULL;
    return fopen(logFile,"a+");
}

void shoit_misc_log(FILE *fp,char *format, ...)
{

    if(!fp) return;

    va_list arglist;
    va_start( arglist, format);
    vfprintf(fp, format, arglist);
    fprintf(fp, "\n");
    va_end(arglist);
}

void shoit_misc_close_log(FILE *fp)
{
    fflush(fp);
    if(fp) fclose(fp);
    fp=NULL;
}

uint64_t shoit_misc_htonll(uint64_t lll)
{
    uint64_t nll = 0;
    unsigned char *cp = (unsigned char *)&nll;

    cp[0] = (lll >> 56) & 0xFF;
    cp[1] = (lll >> 48) & 0xFF;
    cp[2] = (lll >> 40) & 0xFF;
    cp[3] = (lll >> 32) & 0xFF;
    cp[4] = (lll >> 24) & 0xFF;
    cp[5] = (lll >> 16) & 0xFF;
    cp[6] = (lll >>  8) & 0xFF;
    cp[7] = (lll >>  0) & 0xFF;
    return nll;
}

uint64_t shoit_misc_ntohll(uint64_t nll )
{
    unsigned char *cp = (unsigned char *)&nll;

    return ((long long)cp[0] << 56) |
        ((long long)cp[1] << 48) |
        ((long long)cp[2] << 40) |
        ((long long)cp[3] << 32) |
        ((long long)cp[4] << 24) |
        ((long long)cp[5] << 16) |
        ((long long)cp[6] <<  8) |
        ((long long)cp[7] <<  0);
}

int shoit_misc_report_time(struct timeval *start)
{
    struct timeval end;
    int usecs;

    gettimeofday(&end, NULL);
    usecs = 1000000 * (end.tv_sec - start->tv_sec) + (end.tv_usec - start->tv_usec);
    start->tv_sec = end.tv_sec;
    start->tv_usec = end.tv_usec;

    return usecs;
}


void shoit_misc_daemon(char *work_dir)
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0){
        perror("fork");
        exit(1);
    }

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(0);

    /* On success: The child process becomes session leader */
    if (setsid() < 0){
        perror("setsid");
        exit(1);                                                                                                                     
    }                  
    /* Catch, ignore and handle signals */                                                                                           
    //TODO: Implement a working signal handler */                                                                                    
    signal(SIGCHLD, SIG_IGN);                                                                                                        
    signal(SIGHUP, SIG_IGN);                                                                                                         
                                                                                                                                     
    /* Fork off for the second time*/                                                                                                
    pid = fork();                                                                                                                    
                                                                                                                                     
    /* An error occurred */                                                                                                          
    if (pid < 0){                                                                                                                     
        perror("fork");                                                                                                              
        exit(1);                                                                                                                     
    }                                                                                                                                
                                                                                                                                     
    /* Success: Let the parent terminate */                                                                                          
    if (pid > 0)                                                                                                                     
        exit(0);                                                                                                                     
    /* Set new file permissions */                                                                                                   
    umask(0);                                                                                                                        
                                                                                                                                     
    /* Change the working directory to the root directory */                                                                         
    /* or another appropriated directory */                                                                                          
    if(chdir(work_dir) ){                                                                                                            
        perror("chdir");                                                                                                             
        exit(1);                                                                                                                     
    }                                                                                                                                
                                                                                                                                     
    /* Close all open file descriptors */                                                                                            
    int x;                                                                                                                           
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)                                                                                       
    {                                                                                                                                
        close (x);                                                                                                                   
    }                                                                                                                                
                                                                                                                                     
}          


#ifdef TESTING

int main(int arg,char **argv)
{

    char *line= argv[1];
    char host[32];
    int port ;
    printf("input:%s\n",line);

    printf("return:%d\n",shoit_misc_resolve_line(line,host,&port));    
    printf("%s:%d\n",host,port);
    
}




#endif /*endif TESTING*/


