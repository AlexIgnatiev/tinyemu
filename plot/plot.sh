#!/bin/bash

cd opencl/tinysim
git stash
git pull origin elsa
make clean
make

sizes=(1048576 2097152 4194304 8388608 16777216 33554432 67108864 134217728)
iters=11
echo -n "" > cpu.txt
echo -n "" > gpu.txt
for SIZE in ${sizes[@]}
do
    echo -n $SIZE"," >> cpu.txt
    echo -n $SIZE"," >> gpu.txt
	for ((IT=0; IT<$iters; IT++))
    do
        echo -n $(LD_LIBRARY_PATH=/opt/intel/opencl-sdk/lib64/ ./program 1 $SIZE)";" >> cpu.txt
        echo -n $(LD_LIBRARY_PATH=/opt/intel/opencl-sdk/lib64/ ./program 0 $SIZE)";" >> gpu.txt
    done
    echo "" >> cpu.txt
    echo "" >> gpu.txt
done
