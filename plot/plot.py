import matplotlib
import matplotlib.pyplot as plt
import matplotlib.path as mpath
import matplotlib.lines as mlines
import sys
import numpy as np
import os
import subprocess
import csv
from matplotlib.ticker import ScalarFormatter, NullFormatter, FuncFormatter

matplotlib.use('Agg')

path = os.path.join(os.getcwd(), "..")
executable = os.path.join(path, "program") if len(sys.argv) != 2 else sys.argv[1]
num_iters = 1
gpu_legend = mlines.Line2D([], [], color='green', marker='^', markersize=15, label='With GPU validation')
cpu_legend = mlines.Line2D([], [], color='red', marker='o', markersize=15, label='CPU only validation')

def formatter(x, pos=None):
    return str(x) if x < 1 else str(int(x))

def ratio():
    dataset_sizes = [(2**x*1.0)/(1024*1024) for x in range(18, 29)]  # 1KB to 256MB
    ys = []
    num_threads = 1

    rows = [["Dataset size (MB)", "With GPU", "Without GPU", "CPU/GPU ratio"]]

    for i in dataset_sizes:
        print("executing with dataset size={0}".format(i))
        gpu_execs = []
        cpu_execs = []
        row = []
        for __ in range(num_iters):
            repeat = True
            cmd = [executable, "0", str(i * 1024 * 1024), os.path.join(path, "src", "kernels", "main.cl"), str(num_threads)]
            while repeat:
                try:
                    gpu_execs.append(float(subprocess.check_output(cmd)))
                    repeat = False
                except Exception as e:
                    print e
            cmd[1] = "1"  # set flag to use cpu only
            cpu_execs.append(float(subprocess.check_output(cmd)))

        gpu_execs.sort()
        cpu_execs.sort()
        cpu = cpu_execs[num_iters // 2]
        gpu = gpu_execs[num_iters // 2]
        ratio = cpu/gpu if cpu and gpu != 0 else 1
        row = [i, gpu, cpu, ratio]
        ys.append(ratio)
        rows.append(row)

    ax = plt.gca()
    ax.set_xscale('log', basex=2)
    ax.get_xaxis().set_major_formatter(FuncFormatter(formatter))
    #ax.get_xaxis().get_major_formatter().set_scientific(False)

    # Set x logaritmic
    plt.xticks(dataset_sizes)
    ax.set_xlabel('Dataset Size (MB)')
    ax.set_ylabel('clock cycles without/with GPU')    
    plt.plot(dataset_sizes, ys, '--sc', markersize=7)

    #ax.xaxis.ticklabel_format(useMathText=False)
    plt.grid(linestyle="dotted")

    plt.suptitle('{0} threads, using rdtsc'.format(num_threads), fontsize=14, fontweight='bold')

    plt.savefig('plot_{0}threads_ratio_O0.png'.format(num_threads))
    ##plt.show()

    with open("csv_{0}thread_O0.csv".format(num_threads), 'wb') as csv_file:
        writer = csv.writer(csv_file, quoting=csv.QUOTE_MINIMAL)
        writer.writerows(rows)  


def vary_dataset():
    dataset_sizes = [2**x for x in range(8, 29)]  # 1KB to 256MB
    y_with_gpu = []
    y_without_gpu = []
    num_threads = 1

    rows = [["Dataset size (KB)", "With GPU", "Without GPU"]]

    for i in dataset_sizes:
        print("executing with dataset size={0}".format(i))
        gpu_execs = []
        cpu_execs = []
        row = []
        for __ in range(num_iters):
            repeat = True
            cmd = [executable, "0", str(i), os.path.join(path, "src", "kernels", "main.cl"), str(num_threads)]
            while repeat:
                try:
                    gpu_execs.append(float(subprocess.check_output(cmd)))
                    repeat = False
                except Exception as e:
                    print e
            cmd[1] = "1"  # set flag to use cpu only
            cpu_execs.append(float(subprocess.check_output(cmd)))

        gpu_execs.sort()
        cpu_execs.sort()
        row = [i / (1024), gpu_execs[num_iters // 2], cpu_execs[num_iters // 2]]
        y_with_gpu.append(gpu_execs[num_iters // 2])
        y_without_gpu.append(cpu_execs[num_iters // 2])
        rows.append(row)
    
    
    plt.legend(handles=[gpu_legend, cpu_legend])
    
    plt.plot(dataset_sizes, y_with_gpu, '-.^g', markersize=7)
    plt.plot(dataset_sizes, y_without_gpu, '-.or', markersize=7)

    ax = plt.gca()

    # Set x logaritmic
    ax.set_xscale('log', basex=2)
    ax.set_xlabel('Dataset Size (bytes)')
    ax.set_ylabel('time (s)')
    plt.grid(linestyle="dotted")

    plt.suptitle('{0} threads'.format(num_threads), fontsize=14, fontweight='bold')

    plt.savefig('plot_{0}threads_o3.png'.format(num_threads))
    ##plt.show()

    with open("csv_{0}thread.csv".format(num_threads), 'wb') as csv_file:
        writer = csv.writer(csv_file, quoting=csv.QUOTE_MINIMAL)
        writer.writerows(rows)


def _get_cache_level_sz(level):
    lscpu = subprocess.check_output(["lscpu"])
    return int([x for x in lscpu.split("\n") if x.startswith(level)][0].split(" ")[-1][:-1])

def _get_cache_size():
    lscpu = subprocess.check_output(["lscpu"])
    caches = ["L1d", "L2", "L3"]
    sizes_sum = sum([_get_cache_level_sz(x) for x in caches])
    return sizes_sum

def cache_limits():
    cache_sz = (_get_cache_size() * 1024)
    treshold = 4096
    above_cache_sz = cache_sz + treshold
    below_cache_sz = cache_sz - treshold
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
        plt.legend(handles=[gpu_legend, cpu_legend])
        plt.grid(linestyle="dotted")
        plt.suptitle('dataset {0} cache size [{1}/{2}] (all levels full)'.format(arg['name'], arg['value'], cache_sz), fontsize=12, fontweight='bold')
        plt.savefig('plot_dataset_{0}_cache_all.png'.format(arg['name']))

#ratio()
vary_dataset()
#cache_limits()