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
#include "shoit_misc.h"


void shoit_show_copyright()
{
    fprintf(stderr,"\r\n");
    fprintf(stderr,"%s\r\n\r\n",VERSION);
    fprintf(stderr,"***************Warning, Warning, Warning************************\r\n\r\n");
    fprintf(stderr,"%s","SHOIT - A toolkit for High Performance Data transmission.\n");
    fprintf(stderr,"%s","Copyright (C) 2019 Pencode.net\r\n\r\n");
    fprintf(stderr,"*****************************************************************\r\n\r\n");

    fprintf(stderr,"\r\n\r\n");
}

void shoit_sleepto(shoit_core_t *shoit,uint64_t nexttime)
{
    shoit_misc_sleepto(nexttime, &shoit->sendBlockLock,&shoit->sendBlockCond);
}

void shoit_init_synch(shoit_core_t *shoit)
{
    sem_init(&shoit->progSem,0,0);
    sem_init(&shoit->dataSem,0,0);
    if(pipe(shoit->pipe)==-1) perror("pipe");
    pthread_mutex_init(&shoit->sendBlockLock, NULL);
    pthread_cond_init(&shoit->sendBlockCond, NULL);
}

void shoit_destory_synch(shoit_core_t *shoit)
{
    sem_destroy(&shoit->progSem);
    sem_destroy(&shoit->dataSem);
    close(shoit->pipe[0]);
    close(shoit->pipe[1]); 
    pthread_mutex_destroy(&shoit->sendBlockLock);
    pthread_cond_destroy(&shoit->sendBlockCond);
}


void shoit_init_error_bitmap(shoit_core_t *shoit)
{
    uint64_t i;
    char *errorBitmap = (char*)(shoit->errorBitmap + sizeof(shoit_control_t));

    memset(shoit->errorBitmap,0,sizeof(shoit_control_t));
    // the first byte is 0 if there is error.  1 if all done.
    uint64_t startOfLastByte = shoit->totalNumberOfPackets - (shoit->sizeofErrorBitmap-2)*8;
    char bits[8] = {0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080};

    /* The first byte is for judging all_done */
    for (i=0;i<shoit->sizeofErrorBitmap;i++)
        errorBitmap[i] = 0;

    /* Preset those bits unused */
    for (i=startOfLastByte;i<8;i++)
        errorBitmap[shoit->sizeofErrorBitmap-1] |= bits[i];
}


bool shoit_update_error_bitmap(shoit_core_t *shoit,uint64_t seq)
{
    char *errorBitmap = (char*)(shoit->errorBitmap + sizeof(shoit_control_t));
    uint64_t index_in_list, offset_in_index;

    char bits[8] = {0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080};
    if(seq < 0 || (seq >> 3) >= shoit->sizeofErrorBitmap-1) {
        if(shoit->verbose) {
            fprintf(stderr, "seqno <%ld> wrong,out of range:0 ... %ld\n",seq,shoit->sizeofErrorBitmap);
            exit(0);
        }
        return false;
    }
    // seq starts with 0
    index_in_list = seq >> 3; /*8bit*/
    index_in_list ++;
    offset_in_index = seq%8;
    errorBitmap[index_in_list] |= bits[offset_in_index];

    return true;
} 

/*
*return the count of errors
*if update =true, update table
*/
uint64_t shoit_update_hash_table(shoit_core_t *shoit,bool update)
{
    uint64_t count = 0;
    uint64_t i,j;
    char bits[8] = {0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080};
    char *errorBitmap = (char*)(shoit->errorBitmap + sizeof(shoit_control_t));

    for (i=1;i<shoit->sizeofErrorBitmap; i++)
    {
        for (j=0;j<8;j++)
        {
            if ((errorBitmap[i] & bits[j]) == 0)
            {
                if(update)
                    shoit->hashTable[count] = (i-1)*8+j;
                count ++;
            }
        }
    }
    // set the first byte to let the sender know "all done"
    if (count == 0 && update)
        errorBitmap[0] = 1;

    return count;
}


void shoit_set_verbose(shoit_core_t *shoit, int v ) {
    shoit->verbose = v;
}
