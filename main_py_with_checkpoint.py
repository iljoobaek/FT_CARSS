# main_py.c: A python-client program
# 
# Functions used from ft_utils_client.c:
#   1. tag_job_begin
#   2. tag_job_end
#   3. setup_ft_manager
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

libft.setup_ft_manager.restype = c_int
libft.setup_ft_manager.argtypes = [c_uint, c_uint, c_char_p, c_int]

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

# Create python version of setup_ft_manager
def setup_ft_manager_py(pid, tid, job_name, num):
	# print("Call the setup_ft_manager")
	res = c_int(libft.setup_ft_manager(pid, tid, job_name, num))
	return res

# ------------------ checkpoint ----------------------------------------------
def add_1(x):
	x = x + 1
	# print("In add_1")
	return x
def add_2(x):
	x = x + 2
	# print("In add_2")
	return x
def add_3(x):
	x = x + 3
	# print("In add_3")
	return x

# Read from the written log
def get_checkpoint():
	# get x and func from the log
	rd = open("checkpoint.txt", "r")
	line = rd.readline()
	var = line.split()
	# print("line: ")
	# print(var)
	x = var[0]
	func = var[1]
	rd.close()
	print("In checkpoint:")
	print(x, end = ",")
	print(func)
	return int(x), int(func)

# Write to a log
# The log only has one line: x, func
def update_checkpoint(x, func):
	# write to log.txt
	wr = open('checkpoint.txt', 'w')

	string = str(x) + " " + str(func)
	wr.write(string)

	wr.close()
# --------------------------------------------------------

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
	# ft_init_wait()
	res = setup_ft_manager_py(pid, tid, f_name, f_num)


	# ---------- Initial x value or get checkpoint ------------------------
	print(ft_job_name)
	if ft_job_name == "replica":
		x, func = get_checkpoint()

	from pathlib import Path
	my_file = Path("/home/ruiyingw/rw_work/cuMiddleware/checkpoint.txt")
	if ft_job_name == "main":
		if not my_file.is_file():
			# checkpoint.txt doesnt exist, no checkpoint, initialize x 
			x = 0
			func = 4
			print("file checkpoint doesnt exist")
		else :
			# checkpoint.txt exits, use checkpoint to continue the compute
			x, func = get_checkpoint()
			print("Main: get from checkpoint")
	# ---------------------------------------------------------------------

	# Start the work
	print("Start working!")
	print("========================================================")
	for i in range(0,10):
		print("opencv: tag_beginning() %d" %i)
		# Tagging begin
		res = tag_job_begin_py(pid, tid, c_name, fiften, False, True, one)

		## layer-level checkpointing ##
		# QuestionL do I need to know which layer replica should start from ???

		time.sleep(1)

		if func == 1:
			# add_1 has been called
			# call add_2 and add_3
			x = add_2(x);  # mimic layer 1
			print(x , end = '\n')
			update_checkpoint(x, 2); #  ---> checkpoint 2
			time.sleep(1)
			x = add_3(x);  # mimic layer 2
			print(x , end = '\n')
			update_checkpoint(x, 3); #  ---> checkpoint 3
			time.sleep(1)
			func = 4 # avoid calling it again
			continue
		
		if func == 2:
			# add_2 has been called
			# call add_3 
			x = add_3(x);  # mimic layer 2
			print(x , end = '\n')
			update_checkpoint(x, 3); #  ---> checkpoint 3
			time.sleep(1)
			func = 4 # avoid calling it again
			continue

		# Since func = 4, next cycle, it will execute the following only
		x = add_1(x);  # mimic layer 0
		print(x , end = '\n')
		update_checkpoint(x, 1); #  ---> checkpoint 1 
		time.sleep(1)
		x = add_2(x);  # mimic layer 1
		print(x , end = '\n')
		update_checkpoint(x, 2); #  ---> checkpoint 2
		time.sleep(1)
		x = add_3(x);  # mimic layer 2
		print(x , end = '\n')
		update_checkpoint(x, 3); #  ---> checkpoint 3
		time.sleep(1)
		print(" ")
		# Tagging end
		res = tag_job_end_py(pid, tid, c_name)
		time.sleep(1)

