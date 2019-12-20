#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "queue.h"
#include "sem.h"
#include "thread.h"

struct semaphore {
	/* TODO: Phase 1 */
	int accessesLeft; //number of accesses that semaphore has left
	pthread_t *threadToBlock; //thread that is using resource and is blocked
	queue_t waitQueue; //Holds the threads waiting for resource
};

sem_t sem_create(size_t count)
{
	/* TODO: Phase 1 */
	sem_t newSemaphore = malloc(sizeof(sem_t));
	if(newSemaphore == NULL)
	  return NULL;
	
	newSemaphore -> accessesLeft  = (int) count;
	newSemaphore -> waitQueue = queue_create();
	return newSemaphore;
}

int sem_destroy(sem_t sem)
{
	/* TODO: Phase 1 */
	if(sem == NULL || queue_length(sem->waitQueue) > 0) 
	  return -1; 

	free(sem -> threadToBlock);
	queue_destroy(sem -> waitQueue);
	free(sem);
	
	return 0;
}

int sem_down(sem_t sem)
{
	/* TODO: Phase 1 */
	if(sem == NULL)
	  return -1;
	
	enter_critical_section();
	while(sem -> accessesLeft == 0){
	  //Block thread until sem_up & store the thread in a queue 
	  sem -> threadToBlock = (pthread_t*)malloc(sizeof(pthread_t));
	  *(sem -> threadToBlock) = pthread_self();
	  queue_enqueue(sem->waitQueue , (void *)(sem -> threadToBlock));
	  thread_block(); //suspends thread in a block state
	}
	
	sem -> accessesLeft -= 1;
	exit_critical_section();

	return 0;
}

int sem_up(sem_t sem)
{
	/* TODO: Phase 1 */
	if(sem == NULL)
	  return -1;
	
	enter_critical_section();
	sem -> accessesLeft += 1;
	/* Check waiting list, if not empty first thread is unblocked.
	   release semaphore*/
	if(queue_length(sem -> waitQueue) != 0){
	  sem -> threadToBlock = (pthread_t*)malloc(sizeof(pthread_t));
	  *(sem -> threadToBlock) = pthread_self();
	  queue_dequeue(sem->waitQueue , (void**)&(sem -> threadToBlock));
	  thread_unblock(*(sem -> threadToBlock)); //unblock thread for scheduling
	}
	exit_critical_section();

	return 0;
}
