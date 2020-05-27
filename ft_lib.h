/*
	FT(fault tolerance) manager utilise(c) and (c++) common data
	structures and functions

	Data structures:
	- ft_data_t
	- ft_job_t
	- ft_jobs_t
	
	Functions:

	- init_ft_jobs
	- init_ft_data

	Ruiying Wu (ECE)
	5/2020
*/

#ifndef FT_LIB_H
#define FT_LIB_H

#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>

#include <sys/mman.h>     // munmap
#include <fcntl.h>        // for O_ constants, such as "O_RDWR"
#include <errno.h>
#include <sys/types.h> 	  // off_t
#include <sys/mman.h>
#include "mid_common.h"
#include <unistd.h>       //usleep



#define FT_HB_DATA_MAX_DATA 100 // size of heartbeat data array
                                // also the number of heartbeat threads
                                // created by server
/* ft data type */
// At the moment, it only has a heart_beat array, but can add more
// for checkpoint
typedef struct ft_datas{

	unsigned long long heart_beat[FT_HB_DATA_MAX_DATA];

}ft_data_t;

#define FT_DATA_NAME "ft_data"  // name of the heartbeat data 
#define FT_DATA_SIZE sizeof(ft_data_t)

// -------------------------------------------------------------------

/* ft job type */
#define MAX_FT_NAME 100
enum ft_job_type {MAIN, REPLICA};
typedef struct ft_job {

	pid_t pid;						// process id of job
    pid_t tid;                      // tid (since client may be multithreaded)

	char job_name[MAX_FT_NAME];     // name of the job, job_tid
	enum ft_job_type req_type;	    // 'MAIN' or 'REPLICA'
	int num;                        // used to store arg2 as index to heartbeat array
	int is_executed;                // indicate whether the job is executed 

	// Client-side/server-side attrs - communication properties
	sem_t client_wake;				// Semaphore controlling when client can continue within a tag
	bool client_exec_allowed;		// flag determining whether client should execute when woken
} ft_job_t;

// -------------------------------------------------------------------

/* ft jobs type */
#define FT_JOBS_MAX_JOBS 100
#define JOB_MEM_NAME_MAX_LEN 100
#define JOB_MEM_TYPE_MAX_LEN 100

typedef struct ft_jobs {
	int is_active;        // Set to 1 after the server is started;
	int total_count;      // Num of ft jobs
	char job_names[FT_JOBS_MAX_JOBS][JOB_MEM_NAME_MAX_LEN];
	pthread_mutex_t requests_q_lock; // ft_job_thread in server and 
	                                 // main thread in client might access it
} ft_jobs_t;

#define FT_JOBS_NAME "ft_jobs"
#define FT_JOBS_SIZE sizeof(ft_jobs_t)


#define FT_DEBUG_FN(fn, ...) \
{\
	int res;\
	if ((res = fn(__VA_ARGS__)) < 0) {\
		fprintf(stderr, "FT_DEBUG_FN: %s returned %d!\n", #fn, res);\
		assert(false);\
	}\
}

// -------------------------------------------------------------------
// Called in server
/*
 * Name: init_ft_jobs
 * Function: Create a shared memory region for ft-jobs list
 */
int init_ft_jobs(int *fd, ft_jobs_t **addr, bool init_flag)
{
	/* Get access to shared FT_JOBS_SIZE memory region */
	errno = 0;  //  automatically set when error occurs
	// open/create POSIX shared memory; if it doesn't exist, create one.
	*fd = shm_init(FT_JOBS_NAME, FT_JOBS_SIZE);
    if (*fd == -1){
        perror("[Error] in mmap_ft_jobs_queue: shm_init failed");
        return -1;
    }

    *addr = (ft_jobs_t *)mmap(  NULL,
                        FT_JOBS_SIZE,
                        PROT_READ|PROT_WRITE,
                        MAP_SHARED,
                        *fd,
                        0);

    if (*addr == MAP_FAILED) {
    	printf("FT size: %ld\n", FT_JOBS_SIZE);
        perror("[Error] in init_ft_jobs in mmap_ft_jobs_queue: mmap");

        return -1;
    }

	/* Initialize the ft_jobs_t struct in shared memory */
	if (init_flag) {
		ft_jobs_t *fj = *addr;
		fj->is_active = 1;
		fj->total_count = 0;
		// Zero out the ft-jobs' names array. 
		memset(fj->job_names, 0, FT_JOBS_MAX_JOBS * JOB_MEM_NAME_MAX_LEN);

		// Initialize the global lock on the ft_job_names list
		pthread_mutexattr_t mutex_attr;

		// Initialize requests_q_lock to be PROCESS_SHARED
		(void) pthread_mutexattr_init(&mutex_attr);
		(void) pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

		(void) pthread_mutex_init(&(fj->requests_q_lock), &mutex_attr);
	}
	return 0;
}

/* 
 * Name: init_ft_data
 * Function: Create a shared memory region for heartbeat data
 */
int init_ft_data(int *fd, ft_data_t **addr, bool init_flag, int index)
{
	/* Get access to shared FT_DATA_SIZE memory region */
	errno = 0; // automatically set when error occurs
	// Open/create POSIX shared memory; if it doesnt exist, create one.
	*fd = shm_init(FT_DATA_NAME, FT_DATA_SIZE);

	if (*fd == -1) 
	{
		perror("[Error] in mmap_ft_data: shm_init failed");
		return -1;
	}
	*addr = (ft_data_t *)mmap(
								NULL,// kernel chooses the address at which to create the mapping
    							FT_DATA_SIZE,
    							PROT_READ | PROT_WRITE, // pages may be read | pages may be written
    							MAP_SHARED, // sharing this mapping
    							*fd,
    							0);
	if (*addr == MAP_FAILED) 
	{
		perror("[Error] in mmap_ft_data: shm_init failed");
		return -1;
	}

	/* Initialize the ft_data_t struct in shared memory*/
	if (init_flag)
	{
		ft_data_t *ft_data = *addr;
		ft_data->heart_beat[index] = 0; // hardcode heartbeat value for main and replica
	}

	return 0;
} 

#endif 



