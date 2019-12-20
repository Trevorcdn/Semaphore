/*
 * Testing Phase 2.2 on tps.c in libuthread
 *
 * Uses this code plus gcc -o tps_testsuite tps_testsuite.c -Llibuthread -luthread -pthread -Wl,--wrap=mmap
 * 
 * This relies in being able to get a thread's TPS's address so 
 * that you can try to access the TPS page outside of tps_read()/tps_write().
 * 
 * right after a thread creates its TPS, it can get its TPS page address
 * through the global variable (assuming there's no race condition on this 
 * variable from another thread - which should be fine if we test with only one 
 * thread that has a TPS):
 *  
 * Output should be:
 *  TPS protection error!
 *  segmentation fault (core dumped)
 */


#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>

void *latest_mmap_addr; // global variable to make the address returned by mmap accessible

void *__real_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
    latest_mmap_addr = __real_mmap(addr, len, prot, flags, fildes, off);
    return latest_mmap_addr;
}

void *my_thread(void *arg)
{
    char *tps_addr;

    /* Create TPS */
    tps_create();

    /* Get TPS page address as allocated via mmap() */
    tps_addr = latest_mmap_addr;

    /* Cause an intentional TPS protection error */
    tps_addr[0] = '\0';
    
    tps_destroy();
    return NULL;
}

int main(int argc, char **argv)
{
	pthread_t tid;

	/* Init TPS API */
	tps_init(1);
        assert(tps_init(1) == -1);
    
	/* Create thread 1 and wait */
	pthread_create(&tid, NULL, my_thread, NULL);
	pthread_join(tid, NULL);

	return 0;
}
