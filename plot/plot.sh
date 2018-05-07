#!/bin/bash

cd opencl/tinysim
git stash
git pull origin elsa
make clean
make
PROB=100
GPU_FILENAME=gpu_p$PROB.txt
CPU_FILENAME=cpu_p$PROB.txt

sizes=(65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216 33554432 67108864 134217728)
iters=11
echo -n "" > $CPU_FILENAME
echo -n "" > $GPU_FILENAME
for SIZE in ${sizes[@]}
do
    echo -n $SIZE"," >> $CPU_FILENAME
    echo -n $SIZE"," >> $GPU_FILENAME
	for ((IT=0; IT<$iters; IT++))
    do
        echo -n $(LD_LIBRARY_PATH=/opt/intel/opencl-sdk/lib64/ ./program -s $SIZE -d cpu -p $PROB)";" >> $CPU_FILENAME
        echo -n $(LD_LIBRARY_PATH=/opt/intel/opencl-sdk/lib64/ ./program -s $SIZE -d gpu -p $PROB)";" >> $GPU_FILENAME
    done
    echo "" >> $CPU_FILENAME
    echo "" >> $GPU_FILENAME
done
