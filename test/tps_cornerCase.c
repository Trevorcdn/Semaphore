/*
 * Testing cornerCases on tps.c in libuthread
 *
 * Test what happens if we destroy,read, or write from a non-existent tps.
 * Also, tries to do tps_create and tps_init again, which should give errors.
 * 
 * All tps_read and tps_write have been adjusted to have an offset of 3.
 * Errors like NULL buffers and out of bound are tested on both read and write 
 * 
 * Lastly, cloning something that already exist should also give errors
 */

#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

static char msg1[TPS_SIZE] = "Hello world!\n";
static char msg2[TPS_SIZE] = "hello world!\n";

static sem_t sem1, sem2;

void *thread2(void* arg)
{
	char *buffer = malloc(TPS_SIZE);

	/* Create TPS and initialize with *msg1 */
	assert(tps_destroy() == -1); //Destroying non-existent tps
	assert(tps_read(0, TPS_SIZE, msg1) == -1); //read with non-existent tps
	assert(tps_write(0, TPS_SIZE, msg1) == -1);//write with non-existent tps
	
	tps_create();
	assert(tps_create() == -1); //Trying to tps_create again should give error
	
	assert(tps_write(TPS_SIZE, TPS_SIZE, msg1) == -1);//out of bound write
	assert(tps_write(0, TPS_SIZE, NULL) == -1); //NULL buffer
	tps_write(3, TPS_SIZE-3, msg1);
	/* Read from TPS and make sure it contains the message */
	memset(buffer, 0, TPS_SIZE);
	assert(tps_read(TPS_SIZE, TPS_SIZE, msg1) == -1);//out of bound read
	assert(tps_read(0, TPS_SIZE, NULL) == -1);//NULL buffer
	tps_read(3, TPS_SIZE-3, buffer);
	assert(!memcmp(msg1, buffer, TPS_SIZE));
	printf("thread2: read OK!\n");

	/* Transfer CPU to thread 1 and get blocked */
	sem_up(sem1);
	sem_down(sem2);

	/* When we're back, read TPS and make sure it sill contains the original */
	memset(buffer, 0, TPS_SIZE);
	
	assert(tps_read(TPS_SIZE, TPS_SIZE, msg1) == -1);//out of bound read
	assert(tps_read(0, TPS_SIZE, NULL) == -1);//NULL buffer
	tps_read(3, TPS_SIZE-3, buffer);
	
	assert(!memcmp(msg1, buffer, TPS_SIZE));
	printf("thread2: read OK!\n");

	/* Transfer CPU to thread 1 and get blocked */
	sem_up(sem1);
	sem_down(sem2);

	/* Destroy TPS and quit */
	tps_destroy();
	return NULL;
}

void *thread1(void* arg)
{
	pthread_t tid;
	char *buffer = malloc(TPS_SIZE);
    
	
        assert(tps_destroy() == -1); //Destroying non-existent tps
	assert(tps_read(0, TPS_SIZE, msg1) == -1); //read with non-existent tps
	assert(tps_write(0, TPS_SIZE, msg1) == -1);//write with non-existent tps
	
	/* Create thread 2 and get blocked */
	assert(tps_clone(tid) == -1); //cloning something that doesn't exist
	pthread_create(&tid, NULL, thread2, NULL);
	
	sem_down(sem1);

	/* When we're back, clone thread 2's TPS */
	tps_clone(tid);
        assert(tps_clone(tid) == -1); //cloning something that already exist
    
	/* Read the TPS and make sure it contains the original */
	memset(buffer, 0, TPS_SIZE);
	assert(tps_read(TPS_SIZE, TPS_SIZE, msg1) == -1);//out of bound read
	assert(tps_read(0, TPS_SIZE, NULL) == -1);//NULL buffer
	tps_read(3, TPS_SIZE-3, buffer);
	
	assert(!memcmp(msg1, buffer, TPS_SIZE));
	printf("thread1: read OK!\n");

	/* Modify TPS to cause a copy on write */
	buffer[0] = 'h';
	assert(tps_write(TPS_SIZE, TPS_SIZE, msg1) == -1);//out of bound write
	assert(tps_write(0, TPS_SIZE, NULL) == -1); //NULL buffer
	tps_write(3, 1, buffer);

	/* Transfer CPU to thread 2 and get blocked */
	sem_up(sem2);
	sem_down(sem1);

	/* When we're back, make sure our modification is still there */
	memset(buffer, 0, TPS_SIZE);
	
	assert(tps_read(TPS_SIZE, TPS_SIZE, msg1) == -1);//out of bound read
	assert(tps_read(0, TPS_SIZE, NULL) == -1);//NULL buffer
	tps_read(3, TPS_SIZE-3, buffer);
	
	assert(!strcmp(msg2, buffer));
	printf("thread1: read OK!\n");

	/* Transfer CPU to thread 2 */
	sem_up(sem2);

	/* Wait for thread2 to die, and quit */
	pthread_join(tid, NULL);
	tps_destroy();
	return NULL;
}

int main(int argc, char **argv)
{
	pthread_t tid;

	/* Create two semaphores for thread synchro */
	sem1 = sem_create(0);
	sem2 = sem_create(0);
	
	/* Init TPS API */
	tps_init(1);
        assert(tps_init(1) == -1); //trying to init again
    
	/* Create thread 1 and wait */
	pthread_create(&tid, NULL, thread1, NULL);
	pthread_join(tid, NULL);

	/* Destroy resources and quit */
	sem_destroy(sem1);
	sem_destroy(sem2);
	return 0;
}
