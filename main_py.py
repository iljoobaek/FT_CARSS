# main_py.c: A python-client program
# 
# Functions used from ft_utils_client.c:
#   1. tag_job_begin
#   2. tag_job_end
#   3. ft_init_wait
# Ruiying Wu (ECE)
# 5/2020

import os         # os.getpid()
import sys
import functools
from ctypes import cdll, c_uint, c_int, c_void_p, c_char_p, c_ulonglong, c_double, c_longlong, c_bool
import time       # time.sleep()

# Create interface for tag functions
libc = cdll.LoadLibrary('libc.so.6')
libft = cdll.LoadLibrary("/home/ruiyingw/rw_work/cuMiddleware/lib/libft.so")

# Modify the res and argtypes of tag function interface
libft.tag_job_begin.restype = c_int
libft.tag_job_begin.argtypes = [c_uint, c_uint, c_char_p, c_longlong,
								c_bool, c_bool, c_ulonglong]
libft.tag_job_end.restype = c_int
libft.tag_job_end.argtypes = [c_uint, c_uint, c_char_p]

libft.ft_init_wait.restype = c_int
libft.ft_init_wait.argtypes = [c_uint, c_uint, c_char_p, c_int]

# Create python version of tag_job_begin
def tag_job_begin_py(pid, tid, job_name, slacktime, first_flag, shareable_flag,required_mem):
	# print("Call the tag_job_begin")
	res = c_int(libft.tag_job_begin(pid, tid, job_name, slacktime, first_flag, shareable_flag,required_mem))
	return res

# Create python version of tag_job_end
def tag_job_end_py(pid, tid, job_name):
	# print("Call the tag_job_end")
	res = c_int(libft.tag_job_end(pid, tid, job_name))
	return res

# Create python version of ft_init_wait
def ft_init_wait_py(pid, tid, job_name, num):
	# print("Call the ft_init_wait")
	res = c_int(libft.ft_init_wait(pid, tid, job_name, num))
	return res

if __name__ == "__main__":
	# Get input arguments
	ft_job_name = sys.argv[1]
	f_name = c_char_p(ft_job_name.encode('utf-8'))

	ft_job_num = sys.argv[2]
	f_num = int(ft_job_num)
	
	# Get tid and pid
	SYS_gettid = 186 # SYS_gettid
	tid = c_uint(libc.syscall(SYS_gettid))
	pid = c_uint(os.getpid())

	# Create name of the job
	job_name = "opencv_get_image"
	c_name = c_char_p(job_name.encode('utf-8'))

	# Convert number 1 and 15 into c type
	one = c_ulonglong(1) 
	fiften = c_longlong(15)

	# Set up the FT manager
	res = ft_init_wait_py(pid, tid, f_name, f_num)

	# Start the work
	print("Start working!")
	print("========================================================")
	for i in range(0,10):
		print("opencv: tag_beginning() %d" %i)
		# Tagging begin
		res = tag_job_begin_py(pid, tid, c_name, fiften, False, True, one)

		time.sleep(2)

		# Tagging end
		res = tag_job_end_py(pid, tid, c_name)
		time.sleep(1)

