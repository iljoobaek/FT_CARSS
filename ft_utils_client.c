/*
	FT(fault tolerance) manager utilise(c) on Client side
	Following shows how these functions are related to each other:
	
	- setup_ft_manager  --- called in client
	   - tag_ft_job_begin                
	     - init_ft_jobs
	     - build_ft_job
	     - submit_ft_job
	     - sem_wait
	     - sem_destroy
	     - destroy_ft_job
	   - init_ft_hb
	     - heartbeat_thread
	       - init_ft_data

	Ruiying Wu (ECE)
	5/2020
*/
#ifndef FT_UTILS_CLIENT
#define FT_UTILS_CLIENT

#include <assert.h>				// assert()
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <stdbool.h>
#include <semaphore.h>			// sem_t, sem_*()
#include "ft_lib.h"             // ft_data_t, ft_job_t, ft_jobs_t, 
                                // init_ft_data, init_ft_jobs

static int client_FT_fd = 0;    // fd pointing to heartbeat shared region
static ft_data_t *client_FT_data = NULL; // heartbeat data structure


/*
 * Name: heartbeat_thread
 * Function: Update the heartbeat every 1ms when the client is alive
 * Input: index, which indicates which heartbeat is for this client
 */
void* heartbeat_thread(void *varpg)
{
	// Store client arg2 as index in heartbeat array
	int index = *(int *)varpg;

	/* FT hearbeat checker */
	/* First, init FT data if not already*/
	if (client_FT_data == NULL) {
		FT_DEBUG_FN(init_ft_data, &client_FT_fd, &client_FT_data, false, index);
		close(client_FT_fd); 
	}

	/* Next, update the heart beat in the shared memory*/
	clock_t start, end;
	
	double time_passed;
	struct timespec time_sleep = {0};

	start = clock();

	while(1){

		if(client_FT_data->heart_beat[index] > 100){
			// Refresh the heartbeat when it is bigger than 100
			client_FT_data->heart_beat[index] = 0;
		}
		else{
			// Increment heartbeat every 1ms
			client_FT_data->heart_beat[index] ++;
		}

		end = clock();
		time_passed = ((double)(end - start)) * 1000 /CLOCKS_PER_SEC;
		
		// Sleep for the resest of 1ms using nanosleep
		time_sleep.tv_sec = 0;
		time_sleep.tv_nsec = (1 - time_passed)*1000000;

		nanosleep(&time_sleep, NULL);

		start = clock();
	}
	
	return NULL;
}

/*
 * Name: init_ft_hb
 * Function: Create a thread that keeps updating heartbeat
 */

int init_ft_hb(int index)
{
	pthread_t helper_thread;

	printf("Creating the heartbeat thread...\n");
	int * idx = (int *) malloc(sizeof(int));
	*idx = index;
	pthread_create(&helper_thread, NULL, heartbeat_thread, (void *)idx);
	printf("Created the heart beat thread...\n\n");
	return 0;
}

//==========================================================================================================
/*
 * Name: submit_ft_job
 * Function: add a ft job's name to ft-jobs list
 */
void submit_ft_job(ft_jobs_t *fj, ft_job_t *new_job, char *job_name){
	// First, grab the requests_q_lock to modify job_name list
	pthread_mutex_lock(&(fj->requests_q_lock));

	// Then modify job_names
	strncpy(fj->job_names[fj->total_count], job_name, JOB_MEM_NAME_MAX_LEN);
	fj->total_count += 1;
	printf("Added FT job (%s)\n\n", fj->job_names[fj->total_count-1]);

	// Lastly, unlock
	pthread_mutex_unlock(&(fj->requests_q_lock));
	return;
}

/*
 * Name: build_ft_job
 * Function: Create a shared memory for a FT job based on its job name
 */
int build_ft_job(pid_t pid, pid_t tid, const char *job_name,
				enum ft_job_type curr_type, ft_job_t **save_new_job,
				char *copy_ft_job_name, int num){
	
	// First construct FT job name for shared memory region
	char ft_job_name[JOB_MEM_NAME_MAX_LEN];
	sprintf(ft_job_name, "%s_%d", job_name, tid);

	// Then, Get fd for shared memory region designated
	errno = 0;    // automatically set when error occurs
	int fd = shm_init(ft_job_name, sizeof(ft_job_t));
	if(fd == -1){
		perror("[Error] in build_ft_job: shm_init failed");
        return -1;
	}

	// Next, mmap the ft_job_t object stored in shared memory with the fd
	ft_job_t *ft_job = (ft_job_t *)
						mmap(NULL,
							sizeof(ft_job_t),
							PROT_READ|PROT_WRITE,
                        	MAP_SHARED,
                        	fd,
                        	0);
	
	// Close opended fd now that job is mapped
	FT_DEBUG_FN(close, fd);

	if(ft_job == MAP_FAILED) {
		perror("[Error] in mmap_ft_jobs: mmap");
		return -1;
	}

	if(!ft_job) return -1;

	/* Save ft_job and job_name for caller */
	// First, init job with metadata
	ft_job->pid = pid;
	ft_job->tid = tid;
	strncpy(ft_job->job_name, ft_job_name, JOB_MEM_NAME_MAX_LEN);
	ft_job->req_type = curr_type;
	ft_job->num = num;// store arg2 value
	ft_job->is_executed = 0;

	// Second, init client-server semaphore and state
	int pshared = 1; // If pshared is nonzero, then the semaphore is shared between
                     // processes, and should be located in a region of shared memory
	
	if(sem_init(&(ft_job->client_wake), pshared, 0U)){ //0U is the initialized value
		fprintf(stderr, "Failed to init semaphore for ft job!\n");
		return -1;
	}
	ft_job->client_exec_allowed = true;

	*save_new_job = ft_job;
	strncpy(copy_ft_job_name, ft_job_name, JOB_MEM_NAME_MAX_LEN);
	return 0;
}

/*
 * Name: destroy_ft_job
 * Function: release memory mapped for a ft job pointed to by ft_job_ptr
 * Return: 0 on success, negative on error
 */
int destroy_ft_job(ft_job_t **ft_job_ptr) {
	if (ft_job_ptr && *ft_job_ptr) {
		int res;
		if ((res = munmap(*ft_job_ptr, sizeof(ft_job_t))) == 0) {
			*ft_job_ptr = NULL;
		}
		return res;
	}
	return -2;
}

static ft_jobs_t *ft_jobs = NULL;
static int ft_fd = 0;

/*
 * Name: tag_ft_job_begin
 * Function: Create a shared memory for ft job,
 *           Add job's name to ft-jobs list, 
 *           Wait for the wakeup.
 */
int tag_ft_job_begin(pid_t pid, pid_t tid, 
	const char* ft_job_name, int num){
	
	/* First, init ft jobs if not already */
	if (ft_jobs == NULL) {
		FT_DEBUG_FN(init_ft_jobs, &ft_fd, &ft_jobs, false);
		close(ft_fd);
	}

	/* Next, build a ft_job_t in shared memory */
	char job_name[JOB_MEM_NAME_MAX_LEN];
	ft_job_t *tagged_job;

	// Create a label for ft job based on the command
	enum ft_job_type curr_type;
	
	if(!strcmp(ft_job_name, "main"))
		curr_type = MAIN;
	else if(!strcmp(ft_job_name, "replica"))
		curr_type = REPLICA;
	else{
		fprintf(stderr, "Arg1 is wrong, please type <main> or <replica>\n");
		return -1;
	}

	// Create and init ft job shared memory in build_ft_job
	FT_DEBUG_FN(build_ft_job, pid, tid, ft_job_name,
			curr_type,
			&tagged_job,
			job_name, num);

	/* Then, enqueue name of ft job to ft-jobs list */
	submit_ft_job(ft_jobs, tagged_job, job_name);
    printf("Submitted FT job to ft-jobs list.\n\n");
	/* 
	 * Finally, sem_wait on tagged_job, will wake when server allows client 
	 * to wake or notifies client to wake
	 */
	printf("Waiting FT job (%s) be waked...\n", tagged_job->job_name);
	sem_wait(&(tagged_job->client_wake));
	printf("Waked up FT job (%s)\n\n", tagged_job->job_name);

	// Destroy the sem
	sem_destroy(&(tagged_job->client_wake)); //

	// Save exec flag
	bool exec_allowed = tagged_job->client_exec_allowed;

	/* 
	 * On wake, unmap this shared memory,
	 * removing clients reference to shm object 
	 */
	fprintf(stdout, "Destroying shared job...\n");
	FT_DEBUG_FN(destroy_ft_job, &tagged_job);
	fprintf(stdout, "Destroyed shared job\n\n");

	/* 
	 * Check execution flag - return 0 on success (can run) or -1
	 * on error or unallowed to run.
	 */
	if(!exec_allowed){
		fprintf(stderr, "(TID: %d) Aborting job with name %s!\n", tid, ft_job_name);
		return -1;
	} else {
		return 0;
	}

}

//==========================================================================================================

/*
 * Name: setup_ft_manager
 * Function: Called in client program to setup ft manager:
 *           1. Wait to be triggered by server
 *           2. Keep updating heartbeat 
 */
int setup_ft_manager(pid_t pid, pid_t tid, 
	const char* ft_job_name, int num){
	
	int res;
	
	/* Add to ft-jobs list and wait to be triggered by server*/
	res = tag_ft_job_begin(pid, tid, ft_job_name,num);
	if(res < 0) {
		fprintf(stderr, "Failed to tag fit job");
		return EXIT_FAILURE;
	}

	/* Keep updating heartbeat*/
	if((res = init_ft_hb(num)) < 0) 
	{
		fprintf(stderr, "Failed to init ft");
		return EXIT_FAILURE;
	}
	return 0;

}

#endif


















