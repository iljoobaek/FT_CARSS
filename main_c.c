 // main_py.c: A python-client program
 //
 // Ruiying Wu (ECE)
 // 5/2020

#include <stdio.h> //fprintf
#include <unistd.h> // sleep()
#include <sys/types.h> // pid_t
#include "tag_frame.h" // FrameController
#include "tag_gpu.h"

#include "ft_utils_client.c"


int main(int argc, char **argv) {

	/* get input argument, ft job name, replica or main */
	// get ft job name
	const char *ft_job_name = argv[1];
	printf("arg 1: %s\n", ft_job_name);
	
	// get the number of job, 0,1,2,3,4...
	const char *num_str = argv[2];
	int num = atoi(num_str);
	printf("arg 2: %d\n\n", num);

	// get pid and tid of the job
	pid_t pid = getpid();
	pid_t tid = gettid();

	/* Init FT manager and wait for the wake up from the server */
	int res;
	res = ft_init_wait(pid, tid, ft_job_name, num);
	
	printf("Start working!\n");
    printf("========================================================\n");

	const char *job_name = "opencv_get_image";
	for (int i = 0; i < 10; i++)
	{
		fprintf(stdout, "opencv: tag_beginning() %d\n", i);
		// Tagging begin ///////////////////////////////////////////////////////////////////
		res = tag_job_begin(pid, tid, job_name, 
				15L, false, true, 1UL);


        sleep(2);

		// Tag_end /////////////////////////////////////////////////////////////////////////
		tag_job_end(pid, tid, job_name);

		fprintf(stdout, "opencv_get_image: onto next job in 1s\n");
		sleep(1);
	}

}
