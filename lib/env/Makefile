LIBS=-lOpenCL
CCFLAGS=-O0 -g -Wall -std=c11
CC=gcc
DEFINES=-DENABLE_KERNEL_PROFILER

ifeq ($(OS), Windows_NT)
	ifeq ($(shell uname -o), Cygwin)
		CCFLAGS += -D CYGWIN
		LINKS=-L"/cygdrive/c/Intel/OpenCL/sdk/lib/x64"
		INCLUDES=-I"/cygdrive/c/Intel/OpenCL/sdk/include" -Iinclude
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

	LINKS=-L"/opt/intel/opencl/SDK/lib64/"
	INCLUDES=-I/opt/intel/opencl/include/ -I"/opt/intel/opencl/SDK/include/" -Iinclude
	LIBS += -lm -lrt
	CCFLAGS += -Wno-deprecated-declarations
endif

OBJ=env.o
BIN=libenv.so

all: $(OBJ)
	ar rcs libenv.a $(OBJ)

$(OBJ): src/env.c
	$(CC) -c $(DEFINES) $(CCFLAGS) $(INCLUDES) -o $(OBJ) $<

clean:
	rm $(OBJ) libenv.a
