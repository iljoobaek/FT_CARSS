/*
	FT(fault tolerance) manager utilise(c++) on Server side
	Following shows how these functions are related to each other:
	
	- launch_ft_man  --- called in server
	   - ft_jobs_thread                
	     - peek_ft_job_queued_at_i
	     	- get_ft_job
	     - trigger_ft_job
	   - ft_hb_thread

	Ruiying Wu (ECE)
	5/2020
*/
#ifndef FT_UTILS_SERVER
#define FT_UTILS_SERVER

#include <unordered_map>	// std::unordered_map
#include <semaphore.h>	    // sem_t, sem_*()
#include "ft_lib.h"         // ft_data_t, ft_job_t, ft_jobs_t, 
                            // init_ft_data, init_ft_jobs

static int FT_fd = 0;       // fd pointing to heartbeat shared memory region
static ft_data_t *FT_data = NULL; // heartbeat data structure

pthread_mutex_t lock;// lock for running and sleeping job list

/* Name: trigger_ft_job
 * Function: Wake client with ability to run job
 * Input: tj, which is a ft job
 */
int trigger_ft_job(ft_job_t *tj) {
	if (!tj) return -1;
	
	// Set client's execution flag to run
	tj->client_exec_allowed = true;
	return sem_post(&(tj->client_wake));// Unlock the semophora
}

/*
 * Name: get_ft_job
 * Function: get the ft job based on its name
 * Input: name, which is the name of a ft job
 * Return: **save_job, which stores the address of a ft job
 */
int get_ft_job(const char *name, ft_job_t **save_job)
{
    errno = 0;  // Automatically set when error occurs
    int fd = shm_init(name, sizeof(ft_job_t));
    if (fd == -1){
        perror("[Error] in FT get_ft_job: shm_init failed");
        return -1;
    }

    printf("Getting FT job (%s, fd = %d)\n", name, fd);

	// Mmap the job_t object stored in shared memory
	ft_job_t *ft_job = (ft_job_t *)
						mmap(  
						NULL,
                        sizeof(ft_job_t),
                        PROT_READ|PROT_WRITE,
                        MAP_SHARED,
                        fd,
                        0);

    if (ft_job == MAP_FAILED) {
        perror("[Error] in get_ft_job in mmap_ft_jobs_queue: mmap");
        return -1;
    }  

	// Client opened the shared memory, it should still have a reference to
	// This only destroys local connection to shared memory object
	shm_destroy(name, fd);
	*save_job = ft_job;
 	return 0;
}

/*
 * Name: peek_ft_job_queued_at_i
 * Function: called in ft_jobs_thread to get a ft job with a name in ft-jobs list
 * Input: fj, which is the ft-jobs list; qd_job, which is the ft job found; i, 
 * index to get a job name
 * Return: res, indicating whether a ft job is got
 */
int peek_ft_job_queued_at_i(ft_jobs_t *fj, ft_job_t **qd_job, int i)
{
	assert(fj != NULL);
	assert(fj->total_count > 0);
	assert(fj->total_count > i);

	// Grab the name from job_shm_names 
	char *ft_job_mem_name = fj->job_names[i];
	int res = get_ft_job(ft_job_mem_name, qd_job); // Get the ith ft_job
	return res;
}

// Global lists that store either running ft job or sleeping ft job
static std::unordered_map<std::string, ft_job_t*> running_ft_jobs; // store running job
static std::unordered_map<std::string, ft_job_t*> sleeping_ft_jobs; // store sleeping job

// arg1 and arg2 of the job, such as main_0, replica_0
char ft_job_type[JOB_MEM_TYPE_MAX_LEN]; 

static bool ft_continue_flag = true;
/*
 * Name: ft_jobs_thread
 * Fucntion: keep checking the new coming ft jobs and trigger and kill a job based on the
 * the following rule:
 *   1. when main is running, replica is sleeping
 *   2. when main is killed, replica will be run
 *   3. when main is restarted, replica will be killed
 * Input: FJ, which is the ft-jobs list
 */
void *ft_jobs_thread(void *FJ)
{	
	ft_jobs_t *curr_FJ = (ft_jobs_t*)FJ;
	// Begin moving ft jobs from ft_job list to E_list or S_list
	// check every __50_ us
	while (ft_continue_flag)
	{	
		
		/* FT job reader */
		// Grab ft-jobs list lock before emptying
		pthread_mutex_lock(&(curr_FJ->requests_q_lock));
		int i = 0;

		// Grab all new coming ft jobs based on ft-jobs list
		while ((curr_FJ->total_count - i) > 0)
		{	
			int res;
			ft_job_t *q_job;

			// Peek a ft job from ft-jobs list
			if ((res=peek_ft_job_queued_at_i(curr_FJ, &q_job, i)) < 0)
			{
				fprintf(stderr, "Failed to peek FT job at idx %d. Continuing...\n", i);
			    return 0;
            }
			// Enqueue job request to right list
			if(q_job->req_type == MAIN) {
				// Create a lable with type and num, like main_1, main_2
				sprintf(ft_job_type, "%s_%d", "main",q_job->num);
				// Adding to run list
				running_ft_jobs[ft_job_type] = q_job;
				printf("Adding FT job (%s) with key (%s) to running list\n",\
				 q_job->job_name, ft_job_type);

			}
			else if(q_job->req_type == REPLICA){
				// create lable with type and num, like replica_1, replica_2
				sprintf(ft_job_type, "%s_%d", "replica",q_job->num);
				// adding to sleep list
				sleeping_ft_jobs[ft_job_type] = q_job;
				printf("Adding FT job (%s) with key (%s) to sleeping list\n",\
					q_job->job_name, ft_job_type);

			}
			else// Error
			{
				fprintf(stderr, "Failed to add job request to list.");
			}

			// Continue onto next FT job to process
			i++;
		}
		// Reset total count
		curr_FJ->total_count = 0;
		pthread_mutex_unlock(&(curr_FJ->requests_q_lock)); 

		/* FT trigger and killer */
		i = 0;
		char job_type_m[JOB_MEM_TYPE_MAX_LEN]; // type and number for main
		char job_type_r[JOB_MEM_TYPE_MAX_LEN]; // type and number for replica
		
		/* FT job trigger first checks out the running list to see whether
		 * main_index and replica_index exist
		 */
		while((running_ft_jobs.size() - i) > 0){
			/* Create the key, main_i and replica_i */
			sprintf(job_type_m, "%s_%d", "main",i);
			sprintf(job_type_r, "%s_%d", "replica",i);

			/* Check main and repica in runnint list */
			pthread_mutex_lock(&lock);
			auto it_m = running_ft_jobs.find(job_type_m);
			auto it_r = running_ft_jobs.find(job_type_r);
			pthread_mutex_unlock(&lock);

			i++;

			/* Check wether replica not exists and main exists */
			if(it_r == running_ft_jobs.end() && it_m != running_ft_jobs.end()){
				// There is only main in the running list.
				
				/* Check whether it is the first time to trigger this main */
				if(it_m->second->is_executed == 0)
				{
					// It is the first time to run main. trigger it.
					it_m->second->is_executed = 1;
					
					// Wake client to trigger execution
					if(trigger_ft_job(it_m->second) < 0) 
					{
						fprintf(stderr, "\tFailed to wake FT client!\n");
					}
					fprintf(stdout, "Triggered FT job (%s, pid=%d, tid=%d)\n", \
						it_m->second->job_name, it_m->second->pid, it_m->second->tid);

				}
			}
			else if(it_r != running_ft_jobs.end() && it_m != running_ft_jobs.end())
			{
				// There are both replica and main
				// it means the main is new and replica needs to be killed.
				if(it_r->second->is_executed == 0){
					// Main is killed, replica needs to be wake up

					// MAIN is killed, remove MAIN from running list
					running_ft_jobs.erase(it_m);  

					// Already been waked in FT heartbeat thread, no need 
					// call trigger again, just need set a flag
					it_r->second->is_executed = 1; 
					printf("Triggered FT job (%s, pid=%d, tid=%d)!\n", \
						it_r->second->job_name,it_r->second->pid, it_r->second->tid);
				}
				else { 
					// Replica is running and the main is new
					// Kill the replica and trigger the main
					pthread_mutex_lock(&lock);
					kill(it_r->second->pid, SIGINT); // kill replica
					running_ft_jobs.erase(it_r);     // remove it from running list
					pthread_mutex_unlock(&lock);
					printf("Killed FT job (%s, pid=%d, tid=%d)!\n", \
						it_r->second->job_name,it_r->second->pid, it_r->second->tid);

					it_m->second->is_executed = 1;   // set the flag 
					trigger_ft_job(it_m->second);    // trigger the main
					fprintf(stdout, "Triggered FT job (%s, pid=%d, tid=%d)\n", \
						it_m->second->job_name, it_m->second->pid, it_m->second->tid);
				}
			}
		}
		/* Sleep before starting loop again */
		usleep(SLEEP_MICROSECONDS);// 50 us
	}
	return NULL;
}

//==========================================================================================================
/*
 * Name: ft_hb_thread:
 * Function: Keeps tracking heartbeat for each client.
 * Input: the number of a client, used as index to
 * the heartbeat array
 */
void *ft_hb_thread(void *varpg)
{	
	int index = *((int*)varpg);
	/* First, create a FT data shared memory region*/
	int res;
	ft_data_t *curr_ftdata;

	if((res = init_ft_data(&FT_fd, &FT_data, true, index)) < 0)
	{
		fprintf(stderr, "Failed to init ft data");
		return (void*)EXIT_FAILURE;
	}
	curr_ftdata = FT_data;

	/* Next, keep reading the heart beat value*/
	unsigned long long pre_heart_beat = 0;
	int count = 0;

	// timing variables 
	double time_passed;
	struct timespec time_sleep = {0};
	clock_t start, end; 
	start = clock();

	char ft_job_type[JOB_MEM_TYPE_MAX_LEN]; // store the name of the ft job, such as main_0, replica_0

	/* Keep tracking the heartbeat*/
	while(1)
	{	
		/* Check wether the main is dead*/
		if(pre_heart_beat == curr_ftdata->heart_beat[index]){
			// Count the number of times at which the heartbeat doesnt change
			count ++;
		}
		else {
			// Store the current heartbeat
			pre_heart_beat = curr_ftdata->heart_beat[index]; // store the curr heartbeat as pre heart beat
			count = 0;
		}
		
		if (count == 50 && curr_ftdata->heart_beat[index] != 0)
		{
			// The heartbeat doesnt change for 50 times, the main is supposed to be died,
			// wake up the replica.

			/* Wake up replica */
			// Create the key for the replica to be stored in the running list
			sprintf(ft_job_type,"%s_%d", "replica", index);
			auto it_sleep_r = sleeping_ft_jobs.find(ft_job_type);
			if(it_sleep_r != sleeping_ft_jobs.end()){
				// wake up Replica
				trigger_ft_job(it_sleep_r->second);
				pthread_mutex_lock(&lock);
				running_ft_jobs[ft_job_type] = it_sleep_r->second; // put the replica into running list
				sleeping_ft_jobs.erase(it_sleep_r);                // remove it from sleeping list
				pthread_mutex_unlock(&lock);
				
			}
			count = 0;
		}

		/* Sleep for the resest of 1 ms using nanosleep */
		end = clock();
		time_passed = ((double)(end - start)) * 1000 /CLOCKS_PER_SEC; // ms

		time_sleep.tv_sec = 0;
		time_sleep.tv_nsec = (1 - time_passed)*1000000;

		nanosleep(&time_sleep, NULL);

		start = clock(); // Restart the clock
	}

	return NULL;
}

//==========================================================================================================
/*
 * Name: launch_ft_man
 * Function: The API for the client to launch ft manager to check ft-jobs list and heartbeats
 * Input: FJ, which is a shared ft-jobs list 
 */
int launch_ft_man(ft_jobs_t *FJ){

	pthread_t helper_thread[FT_HB_DATA_MAX_DATA+1]; 
	printf("\tCreating FT jobs thread...\n");
	pthread_create(&helper_thread[0], NULL, ft_jobs_thread, FJ); //. pass FJ job list
	printf("\tCreated FT jobs thread.\n\n");

	printf("\tCreating the heartbeat thread...\n");
	// generate 100 threads for heart beats
	int index[FT_HB_DATA_MAX_DATA];
	int i;
	for(i = 0; i < FT_HB_DATA_MAX_DATA; i++){
		index[i] = i; // set index[i] value as i and pass it to thread
		pthread_create(&helper_thread[i+1], NULL, ft_hb_thread, (void *)(&index[i]));
	}
	printf("\tCreated the heartbeat thread.\n");

	return 0;
}

#endif




















