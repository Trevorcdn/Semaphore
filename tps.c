#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "queue.h"
#include "thread.h"
#include "tps.h"

/* TODO: Phase 2 */

/* Private data strucure tps needs to hold the TID
 * and the data containing page tables
 */
typedef struct pageTable{
	void *data;
	int refCount;
}pages_t;

typedef struct tps{
        pthread_t threadTID;
	struct pageTable *pages;
}tps_t;

/*global queue that holds the tps*/
queue_t tpsHolder;

/*Creates TPS with a unique TID and assign the data to the mmap in tps_create*/
tps_t* createTPS(pthread_t TID, void* map){
	tps_t *newTPS = malloc(sizeof(tps_t));
	pages_t *newPage = malloc(sizeof(pages_t));
	
	newTPS->pages = newPage;
	newTPS->threadTID = TID;
	
	newPage->data = map;
	newPage->refCount = 1;
	
	return newTPS; 
}

/*Func for queue_iterate for finding TID of a TPS*/
static int searchQueue(void *data, void *arg)
{
        struct tps *a = (tps_t*)(data);
	pthread_t match = (pthread_t)arg;
	if(a->threadTID == match){
	   return 1;
	}
	return 0;
}

/*Func for queue_iterate to find if p_fault matches with any TPS areas*/
static int searchPageTable(void *data, void *arg)
{
        struct tps *a = (tps_t*)(data);
	void *match = arg;
	if(a->pages->data == match){
	   return 1;
	}
	return 0;
}

/* Signal handler that will distinguish between real segmentation faults and
 * TPS protection faults
 */
static void segv_handler(int sig, siginfo_t *si, void *context)
{
       /*
        * Get the address corresponding to the beginning of the page where the
        * fault occurred
        */
        void *p_fault = (void*)((uintptr_t)si->si_addr & ~(TPS_SIZE - 1));
        tps_t *tpsPage = NULL;

       /*
        * Iterate through all the TPS areas and find if p_fault 
        * matches one of them
        */
        if(queue_iterate(tpsHolder,searchPageTable,(void *)p_fault,(void**)&tpsPage) != 0)
	   /*if queue_iterate failed*/
           fprintf(stderr, "TPS protection error!\n");
		
	/* There is a match */
        if(tpsPage != NULL)
           /* Printf the following error message */
           fprintf(stderr, "TPS protection error!\n");

        /* In any case, restore the default signal handlers */
        signal(SIGSEGV, SIG_DFL);
        signal(SIGBUS, SIG_DFL);
        /* And transmit the signal again in order to cause the 
         * program to crash */
        raise(sig);
}

int tps_init(int segv)
{
	/* TODO: Phase 2 */
	/*if queue has been initialized*/
	if(tpsHolder != NULL)
	   return -1;
	
	if(segv){
           struct sigaction sa;

           sigemptyset(&sa.sa_mask);
           sa.sa_flags = SA_SIGINFO;
           sa.sa_sigaction = segv_handler;
           sigaction(SIGBUS, &sa, NULL);
           sigaction(SIGSEGV, &sa, NULL);
        }
    
	tpsHolder = queue_create();
	
        /*if queue_create failed*/
	if(tpsHolder == NULL)
	   return -1;

	return 0;
	
}


int tps_create(void)
{
	/* TODO: Phase 2 */
	pthread_t tID = pthread_self();
	tps_t *newTps = NULL;

	if(queue_iterate(tpsHolder,searchQueue,(void *)tID,(void**)&newTps) != 0){
	   /*if queue_iterate failed*/
	   return -1;
	}
	else if(newTps != NULL){
	   /*if current thread already have a TPS*/
	   return -1;
	}
	else{
	   /*Otherwise, create the TPS*/
	   newTps = createTPS(tID,mmap(NULL,TPS_SIZE,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0));
	   
           if(newTps == NULL){
	      exit_critical_section();	
	      return -1;	
	   }
		
	   queue_enqueue(tpsHolder, newTps);
	   
	   return 0;
	}
		
}

int tps_destroy(void)
{
	/* TODO: Phase 2 */
	pthread_t tID = pthread_self();
	tps_t *getTPS = NULL;
	if(queue_iterate(tpsHolder,searchQueue,(void*)tID,(void**)&getTPS) !=0){
	   /*if queue_iterate failed*/
	   return -1;
	}
	else if(getTPS == NULL){
	   /*if current thread does not have a TPS*/
	   return -1;
	}
	else{
	   /*Otherwise, destroy the TPS*/
	   enter_critical_section();
	   queue_delete(tpsHolder, (void *)&getTPS);
	   free(getTPS);
	   if(queue_length(tpsHolder) == 0)
	      queue_destroy(tpsHolder);
	   exit_critical_section(); 
	   return 0;
	}
}

int tps_read(size_t offset, size_t length, char *buffer)
{
	/* TODO: Phase 2 */
	if(buffer == NULL || (offset + length) > TPS_SIZE)
	   return -1;
	   
        pthread_t tID = pthread_self();
        tps_t *getTPS =  malloc(sizeof(tps_t));
	if(getTPS == NULL)
	   return -1;
	   
	int succeed = queue_iterate(tpsHolder,searchQueue,(void*)tID,(void**)&getTPS);
	
	if(succeed != 0 || getTPS->threadTID == 0){
	   /*queue_iterate failed or TPS is not initialized */
	   return -1;
	}
	
	enter_critical_section();
	mprotect(getTPS->pages->data, length, PROT_READ);
	memcpy(buffer, getTPS->pages->data + offset, length);
	mprotect(getTPS->pages->data, length, PROT_NONE);
	exit_critical_section();
	
	return 0;
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	/* TODO: Phase 2 */	
		if(buffer == NULL || (offset + length) > TPS_SIZE)
	   return -1;
	   
	pthread_t tID = pthread_self();
	tps_t *getTPS =  malloc(sizeof(tps_t));
	if(getTPS == NULL)
	   return -1;
	int succeed = queue_iterate(tpsHolder,searchQueue,(void*)tID,(void**)&getTPS);
	if(succeed != 0 || getTPS->threadTID == 0){
	   /* queue_iterate failed or if threadTID == 0 then tps hasn't been 
	    * created yet */
	   return -1;
	}
	
	enter_critical_section();
	if(getTPS->pages->refCount > 1){
	   tps_t *oldTPS = createTPS(tID, mmap(NULL, TPS_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON,-1,0));
	   
           if(oldTPS == NULL){
		exit_critical_section();	 
		return -1;
          }
		
	   mprotect(getTPS->pages->data, length, PROT_WRITE); 
	   mprotect(oldTPS->pages->data, length, PROT_WRITE); 

	   memcpy(oldTPS->pages->data,getTPS->pages->data,TPS_SIZE);
	   oldTPS->pages->refCount--;
	   getTPS->pages->data = oldTPS->pages->data;
		
	   mprotect(oldTPS->pages->data, length, PROT_NONE);
	   exit_critical_section();	
	}
	
	mprotect(getTPS->pages->data, length, PROT_WRITE);  
	memcpy(getTPS->pages->data + offset, buffer , length);
	mprotect(getTPS->pages->data, length, PROT_NONE);
	exit_critical_section();
	
	return 0;

}

int tps_clone(pthread_t tid)
{
	/* TODO: Phase 2 */
	tps_t *copyTPS = malloc(sizeof(tps_t));
	tps_t *memCpyTPS = malloc(sizeof(tps_t));
	
	/*Malloc failure*/
	if(memCpyTPS == NULL || copyTPS == NULL)
	   return -1;
	
	if(queue_iterate(tpsHolder,searchQueue,(void *)tid,(void **)&copyTPS) != 0)
	   return -1;

	if(queue_iterate(tpsHolder,searchQueue,(void *)pthread_self(),(void **)&memCpyTPS) != 0)
	   return -1;
 	
	if(copyTPS->threadTID == 0 || memCpyTPS->threadTID != 0){
	   /* if thread @tid doesn't have a TPS then tid should be 0 cause it 
            * is the main thread, or if current thread already has a TPS then 
            * the tid has to be a unique (int) that isn't 0.
            */
	   return -1;
	}
	
	enter_critical_section();
	
	memCpyTPS = createTPS(pthread_self(), copyTPS->pages->data);
	if(memCpyTPS == NULL)
	   return -1;
	
	copyTPS->pages->refCount++;
	memCpyTPS->pages->refCount++;
	queue_enqueue(tpsHolder, memCpyTPS);
	
	exit_critical_section();
	
	return 0;
	 
}
