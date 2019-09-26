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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shoit_progress.h"

void shoit_progress_init(progress_t *progress, char *title, int max, int style)
{

    //fprintf(stderr,"call func:%s\n",__func__);
    progress->chr = '#';
    progress->title = title;
    progress->style = style;
    progress->max = max;
    progress->offset = 100 / (float)max;
    progress->pro = (char *) malloc(max+1);

    if ( style == PROGRESS_BGC_STYLE ){
        memset(progress->pro,0, max+1);
    }else {
        memset(progress->pro, 32, max);
        memset(progress->pro+max, 0, 1);
    }
}

void shoit_progress_show(progress_t *progress, float bit)
{
    int val = (int)(bit * progress->max);

    switch ( progress->style )
    {
        case PROGRESS_NUM_STYLE:                                                                                                     
            fprintf(stderr,"\033[?25l\033[31m\033[1m%s%d%%\033[?25h\033[0m\r",                                                       
                progress->title, (int)(progress->offset * val));                                                                               
            fflush(stderr);                                                                                                          
            break;                                                                                                                   
                                                                                                                                     
        case PROGRESS_CHR_STYLE:                                                                                                     
            memset(progress->pro, '.', val);                                                                                              
            fprintf(stderr,"\033[?25l\033[31m\033[1m%s  %-s| %d%%\033[?25h\033[0m\r",                                                
                progress->title, progress->pro, (int)(progress->offset * val));                                                                     
            fflush(stderr);                                                                                                          
            break;                                                                                                                   
                                                                                                                                     
        case PROGRESS_BGC_STYLE:                                                                                                     
            memset(progress->pro, 32, val);                                                                                               
            fprintf(stderr,"\033[?25l\033[31m\033[1m%s\033[41m %d%% %s\033[?25h\033[0m\r",                                           
                progress->title, (int)(progress->offset * val), progress->pro);                                                                     
            fflush(stderr);                                                                                                          
            break;                                                                                                                   
    }                                                                                                                                
}            



void shoit_progress_show_all(FILE *fp,progress_t *progress, float bit,long total)               
{                                                                                                                                    
                                                                                                                                     
                                                                                                                                     
    int val = (int)(bit * progress->max);                                                                                                 
    char buffer[512]={0};                                                                                                            

    float mbps = 1e-6 * 8*progress->total / (progress->utime == 0 ? .01 : progress->utime);
                                                                                                                                     
    memset(buffer,0,512);                                                                                                            
    snprintf(buffer,sizeof(buffer)-1,"%gMbps    %ld/%ldK    %.2fs    ",mbps,progress->total>>10,total,progress->utime);                                        
                                                                                                                                     
    int prog = (int)(progress->offset * val);                                                                                             
                                                                                                                                     
    if(prog>100) prog=100;                                                                                                           
                                                                                                                                     
    fprintf(fp,"\033[?25l\033[31m\033[1m%s%d%%\033[?25h\033[0m\r",                                                                   
        buffer, prog);                                                                                                               
    fflush(fp);                                                                                                                      
}                                                                                                                                    

void shoit_progress_for_stream(FILE *fp,progress_t *progress, float bit)
{
    char buffer[512]={0};

    memset(buffer,0,512);
    
    float mbps = 1e-6 * 8*progress->total / (progress->utime == 0 ? .01 : progress->utime);

    snprintf(buffer,sizeof(buffer)-1,"%gMbps    %ldK    %.2fs    ",mbps,progress->total>>10,progress->utime);

    fprintf(fp,"\033[?25l\033[31m\033[1m%s%%\033[?25h\033[0m\r", buffer);
    fflush(fp); 
}



void shoit_progress_destroy(progress_t *progress)                                                                               
{                                                                                                                                    
    free(progress->pro);                                                                                                                  
}                                                                                                                                    
