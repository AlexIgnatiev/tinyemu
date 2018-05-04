LIBS= -lenv -lOpenCL -lpthread
CCFLAGS=-O0 -g -Wall -std=c11
CC=gcc
DEFINES=-DDEBUG -DENABLE_KERNEL_PROFILER

ifeq ($(OS), Windows_NT)
	ifeq ($(shell uname -o), Cygwin)
		CCFLAGS += -D CYGWIN
		LINKS=-L"/cygdrive/c/Intel/OpenCL/sdk/lib/x64" -L"lib/env"
		INCLUDES=-I"/cygdrive/c/Intel/OpenCL/sdk/include" -Iinclude -I"lib/env/include"
	endif
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		CCFLAGS += -D LINUX
	endif
	ifeq ($(UNAME_S),Darwin)
		CCFLAGS += -D MAC
	endif
	UNAME_P := $(shell uname -p)
	ifeq ($(UNAME_P),x86_64)
		CCFLAGS += -D AMD64
	endif
	ifneq ($(filter %86,$(UNAME_P)),)
		CCFLAGS += -D IA32
	endif
	ifneq ($(filter arm%,$(UNAME_P)),)
		CCFLAGS += -D ARM
	endif

	LINKS=-L/opt/intel/opencl/ -Llib/env
	INCLUDES=-I/opt/intel/opencl/include/ -I/opt/intel/opencl-sdk/include/ -Iinclude -Ilib/env/include
endif

ROOT_DIR=src
OBJS=$(patsubst %.c, %.o, $(shell find $(ROOT_DIR) -name '*.c'))
BIN=program

.PHONY = all build build_libs clean

all: build_libs build

build_libs:
	cd lib/env && $(MAKE)

build: $(OBJS)
	$(CC) $(CCFLAGS) $^ $(LINKS) $(LIBS) -o $(BIN) 	

%.o: %.c
	$(CC) -c $(DEFINES) $(CCFLAGS) $(INCLUDES) -o $@ $<

%.o: %.c
	$(CC) -c $(DEFINES) $(CCFLAGS) $(INCLUDES) -o $@ $<


clean:
	rm $(OBJS) $(BIN) && cd lib/env && $(MAKE) clean
