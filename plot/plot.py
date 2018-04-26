import matplotlib.pyplot as plt
import matplotlib.path as mpath
import matplotlib.lines as mlines
import sys
import numpy as np
import os
import subprocess

path = os.path.join(os.getcwd(), "..")
executable = os.path.join(path, "program") if len(sys.argv) != 2 else sys.argv[1]

def vary_dataset():
    dataset_sizes = [2**x for x in range(10, 29)]  # 1KB to 256MB
    y_with_gpu = []
    y_without_gpu = []
    num_iters = 11

    for i in dataset_sizes:
        print("executing with dataset size={0}".format(i))
        gpu_execs = []
        cpu_execs = []
        for __ in range(num_iters):
            repeat = True
            cmd = [executable, "0", str(i), os.path.join(path, "src", "kernels", "main.cl")]
            while repeat:
                try:
                    gpu_execs.append(float(subprocess.check_output(cmd)))
                    repeat = False
                except:
                    pass
            cmd[1] = "1"  # set flag to use cpu only
            cpu_execs.append(float(subprocess.check_output(cmd)))

        gpu_execs.sort()
        cpu_execs.sort()
        y_with_gpu.append(gpu_execs[num_iters // 2])
        y_without_gpu.append(cpu_execs[num_iters // 2])

    gpu_legend = mlines.Line2D([], [], color='green', marker='^', markersize=15, label='With GPU validation')
    cpu_legend = mlines.Line2D([], [], color='red', marker='o', markersize=15, label='CPU only validation')
    
    
    plt.legend(handles=[gpu_legend, cpu_legend])
    
    plt.plot(dataset_sizes, y_with_gpu, '-.^g', markersize=7)
    plt.plot(dataset_sizes, y_without_gpu, '-.or', markersize=7)

    ax = plt.gca()

    # Set x logaritmic
    ax.set_xscale('log', basex=2)
    ax.set_xlabel('Dataset Size (bytes)')
    ax.set_ylabel('time (s)')
    plt.grid(linestyle="dotted")

    plt.suptitle('16 threads; -O3 optimization', fontsize=14, fontweight='bold')

    plt.savefig('plot_16threads_o3.png')
    ##plt.show()

vary_dataset()