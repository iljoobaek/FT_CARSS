ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
GCC=gcc
CXX=g++
CUDAPATH?=/usr/local/cuda-10.0

all: libmid.so libcarss.so libft.so mid

CPP_FLAGS=-std=c++11
SHAREDFLAGS=-Wall -fPIC -shared -ldl -g -lrt
INCL_FLAGS=-I./include
MIDFLAGS=-Wall -g -Wl,--no-as-needed
EDIT_LD_PATH=LD_LIBRARY_PATH=$(ROOT_DIR)/lib
LOAD_MID=-Llib -lmid -lpthread -lrt
MID_LOAD=-lpthread -lrt

libcuhook.so: wrapcuda.cpp common.c mid_queue.c
	$(CXX) $(INCL_FLAGS) -I$(CUDAPATH)/include -o lib/libcuhook.so wrapcuda.cpp common.c mid_queue.c $(SHAREDFLAGS)

common.o: common.c
	$(GCC) $(INCL_FLAGS) $(MIDFLAGS) -o common.o common.c -c -fPIC

mid_queue.o: mid_queue.c
	$(GCC) $(INCL_FLAGS) $(MIDFLAGS) -o mid_queue.o mid_queue.c -c -fPIC

tag_lib.o: tag_lib.cpp
	$(CXX) $(INCL_FLAGS) $(CPP_FLAGS) $(MIDFLAGS) -o tag_lib.o tag_lib.cpp -c -fPIC

libmid.so: mid_queue.o common.o
	$(GCC) -shared -o lib/libmid.so mid_queue.o common.o

tag_state.o: tag_state.cpp
	$(CXX) $(INCL_FLAGS) $(CPP_FLAGS) $(MIDFLAGS) -o tag_state.o tag_state.cpp -c -fPIC

tag_frame.o: tag_frame.cpp
	$(CXX) $(INCL_FLAGS) $(CPP_FLAGS) $(MIDFLAGS) -o tag_frame.o tag_frame.cpp -c -fPIC

libcarss.so: tag_state.o tag_lib.o tag_frame.o libmid.so
	$(EDIT_LD_PATH) $(CXX) -shared -o lib/libcarss.so tag_state.o tag_lib.o tag_frame.o $(LOAD_MID)

test_mid.o: tests/test_mid.c tag_lib.o libmid.so
	$(EDIT_LD_PATH) $(GCC) $(INCL_FLAGS) $(MIDFLAGS) -o tests/test_mid.o tests/test_mid.c tag_lib.o $(LOAD_MID)

test_app1: app1.c tag_lib.o mid_queue.o common.o
	$(EDIT_LD_PATH) $(GCC) $(INCL_FLAGS) $(MIDFLAGS) -o app1.o app1.c tag_lib.o mid_queue.o common.o -lrt -lpthread

test_app2: app2.c tag_lib.o mid_queue.o common.o
	$(EDIT_LD_PATH) $(GCC) $(INCL_FLAGS) $(MIDFLAGS) -o app2.o app2.c tag_lib.o mid_queue.o common.o -lrt -lpthread

test_app3: app3.cpp tag_lib.o mid_queue.o common.o tag_frame.o tag_state.o
	$(EDIT_LD_PATH) $(CXX) $(INCL_FLAGS) $(CPP_FLAGS) $(MIDFLAGS) -o app3.o app3.cpp tag_lib.o mid_queue.o common.o tag_frame.o tag_state.o -lrt -lpthread

test_app4: app4.cpp tag_lib.o mid_queue.o common.o tag_frame.o tag_state.o
		$(EDIT_LD_PATH) $(CXX) $(INCL_FLAGS) $(CPP_FLAGS) $(MIDFLAGS) -o app4.o app4.cpp tag_lib.o mid_queue.o common.o tag_frame.o tag_state.o -lrt -lpthread


test_app: test_app1 test_app2 test_app3 test_app4

################ compile main_c.c.  #####################
test_main_c: main_c.c ft_utils_client.c ft_lib.h tag_lib.o mid_queue.o common.o 
	$(EDIT_LD_PATH) $(GCC) $(INCL_FLAGS) $(MIDFLAGS) -o main_c.o main_c.c tag_lib.o mid_queue.o common.o -lrt -lpthread
# test_replica_c: main_c.c ft_utils_client.c tag_lib.o mid_queue.o common.o 
# 	$(EDIT_LD_PATH) $(GCC) $(INCL_FLAGS) $(MIDFLAGS) -o replica_c.o main_c.c tag_lib.o mid_queue.o common.o -lrt -lpthread



############### libft.so #################




ft_server_lib.o: ft_utils_server.cpp #ft_lib.h
	$(CXX) $(INCL_FLAGS) $(CPP_FLAGS) $(MIDFLAGS) -o ft_server_lib.o ft_utils_server.cpp -c -fPIC
ft_client_lib.o: ft_utils_client.c #ft_lib.h
	$(GCC) $(INCL_FLAGS) $(MIDFLAGS) -o ft_client_lib.o ft_utils_client.c -c -fPIC

libft.so: tag_state.o tag_lib.o tag_frame.o libmid.so ft_server_lib.o ft_client_lib.o
	$(EDIT_LD_PATH) $(CXX) -shared -o lib/libft.so tag_lib.o ft_server_lib.o ft_client_lib.o $(LOAD_MID)



##########################################

run_test_mid: tests/test_mid.o
	$(EDIT_LD_PATH) ./tests/test_mid.o;

test_tag.o: tests/test_tag.c tag_lib.o libmid.so
	$(EDIT_LD_PATH) $(GCC) $(INCL_FLAGS) $(MIDFLAGS) -o tests/test_tag.o tests/test_tag.c tag_lib.o $(LOAD_MID)

run_test_tag: test_tag.o
	$(EDIT_LD_PATH) ./tests/test_tag.o;

test_tag_dec.o: tests/test_tag_dec.cpp tag_state.o tag_lib.o libmid.so
	$(EDIT_LD_PATH) $(CXX) $(INCL_FLAGS) $(MIDFLAGS) -std=c++11 -o tests/test_tag_dec.o tests/test_tag_dec.cpp tag_state.o tag_lib.o $(LOAD_MID)

run_test_tag_dec: tests/test_tag_dec.o
	$(EDIT_LD_PATH) ./tests/test_tag_dec.o

# added ft_utils_server.cpp
mid: mymid.cpp mid_queue.o common.o ft_utils_server.cpp
	$(EDIT_LD_PATH) $(CXX) $(INCL_FLAGS) $(CPP_FLAGS) $(MIDFLAGS) -o mid common.c mymid.cpp mid_queue.c $(MID_LOAD)

runmid: mid
	$(EDIT_LD_PATH) ./mid;

clean:
	rm -rf mid tests/test_tag_dec tests/test_tag
	rm -rf *.o
	rm -rf ./tests/*.o
	rm -rf lib/*.so
