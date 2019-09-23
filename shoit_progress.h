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
#ifndef SHOIT_PROGRESS_H__
#define SHOIT_PROGRESS_H__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>


//some progress code import from github's opensources,thanks author

typedef struct st_progress_t{
    char chr;        /*tip char*/
    char *title;    /*tip string*/
    int style;        /*progress style*/
    int max;        /*maximum value*/
    float offset;
    char *pro;

    uint64_t total;
    float utime; /*cast time,second*/

}progress_t;

#define PROGRESS_NUM_STYLE 0
#define PROGRESS_CHR_STYLE 1
#define PROGRESS_BGC_STYLE 2

extern void shoit_progress_init(progress_t *progress,char *title, int max, int style);
extern void shoit_progress_show(progress_t *progress,float bit);
extern void shoit_progress_show_all(FILE *fp,progress_t *progress, float bit,long total);
extern void shoit_progress_for_stream(FILE *fp,progress_t *progress, float bit);
extern void shoit_progress_destroy(progress_t *progress);

#endif
