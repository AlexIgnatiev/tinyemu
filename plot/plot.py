import matplotlib.pyplot as plt
import matplotlib.path as mpath
import matplotlib.lines as mlines
import sys
import numpy as np
import os
import subprocess

path = os.path.join(os.getcwd(), "..")
executable = os.path.join(path, "program") if len(sys.argv) != 2 else sys.argv[1]
num_iters = 11

def vary_dataset():
    dataset_sizes = [2**x for x in range(10, 29)]  # 1KB to 256MB
    y_with_gpu = []
    y_without_gpu = []

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

    plt.suptitle('1 threads; -O3 optimization', fontsize=14, fontweight='bold')

    plt.savefig('plot_1threads_o3.png')
    ##plt.show()

def _get_cache_size():
    lscpu = subprocess.check_output(["lscpu"])
    return int([x for x in lscpu.split("\n") if x.startswith("L3")][0].split(" ")[-1][:-1])

def cache_limits():
    cache_sz = _get_cache_size() * 1024
    treshold = 1024
    above_cache_sz = cache_sz + 1024
    below_cache_sz = cache_sz - 1024
    threads = [1] + range(2, 18, 2)
    plots = [{'name': 'below', 'value': below_cache_sz},
            {'name': 'exact', 'value': cache_sz},
            {'name': 'over', 'value': above_cache_sz}]

    for plot in enumerate(plots):
        plt.figure(plot[0])
        arg = plot[1]
        print("executing for dataset size {0} cache size".format(arg['name']))
        y_gpu = []
        y_cpu = []
        for nthreads in threads:
            ys_gpu = []
            ys_cpu = []
            for __ in range(num_iters):
                cmd = [executable, "0", str(arg['value']), os.path.join(path, "src", "kernels", "main.cl"), str(nthreads)]
                repeat = True
                while repeat:
                    try:
                        ys_gpu.append(float(subprocess.check_output(cmd)))
                        repeat = False
                    except Exception as e:
                        pass#print e
                cmd[1] = "1"
                ys_cpu.append(float(subprocess.check_output(cmd)))
            ys_cpu.sort()
            ys_gpu.sort()
            y_gpu.append(ys_gpu[num_iters // 2])
            y_cpu.append(ys_cpu[num_iters // 2])
        plt.plot(threads, y_gpu, '-.^g', markersize=7)
        plt.plot(threads, y_cpu, '-.or', markersize=7)
        ax = plt.gca()
        ax.set_xlabel('Number of CPU threads')
        ax.set_ylabel('time (s)')
        plt.grid(linestyle="dotted")
        plt.suptitle('dataset {0} cache size [{1}/{2}]'.format(arg['name'], arg['value'], cache_sz), fontsize=14, fontweight='bold')
        plt.savefig('plot_dataset_{0}_cache.png'.format(arg['name']))


#vary_dataset()
cache_limits()