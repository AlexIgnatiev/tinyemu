import matplotlib
import matplotlib.pyplot as plt
import matplotlib.path as mpath
import matplotlib.lines as mlines
import sys
import numpy as np
import os
import subprocess
import csv
from matplotlib.ticker import FuncFormatter

gpu_legend = mlines.Line2D([], [], color='green', marker='^', markersize=15, label='With GPU validation')
cpu_legend = mlines.Line2D([], [], color='red', marker='o', markersize=15, label='CPU only validation')
num_threads = 1

def formatter(x, pos=None):
    return str(x) if x < 1 else str(int(x))


def vary_dataset_size(gpu_input_f="gpu.txt", cpu_input_f="cpu.txt"):
    sizes = []
    y_cpu = []
    y_gpu = []
    with open(cpu_input_f, 'rb') as cpu_file, open(gpu_input_f, 'rb') as gpu_file:
        cpu_reader = csv.reader(cpu_file)
        gpu_reader = csv.reader(gpu_file)
        for row in cpu_reader:
            sizes.append(float(row[0]) / 1024 / 1024)
            floats = []
            outputs = row[1].split(";")[:-1]
            for f in outputs:
                floats.append(float(f))
            floats.sort()
            y_cpu.append(floats[cpu_reader.line_num // 2])

        for row in gpu_reader:
            floats = []
            outputs = row[1].split(";")[:-1]
            for f in outputs:
                floats.append(float(f))
            floats.sort()
            y_gpu.append(floats[gpu_reader.line_num // 2])
    
    ax = plt.gca()
    ax.set_xscale('log', basex=2)
    ax.get_xaxis().set_major_formatter(FuncFormatter(formatter))
    #ax.get_xaxis().get_major_formatter().set_scientific(False)

    # Set x logaritmic
    plt.xticks(sizes)
    ax.set_xlabel('Dataset Size (MB)')
    ax.set_ylabel('time (s)')    
    plt.legend(handles=[gpu_legend, cpu_legend])
    
    print sizes
    print y_gpu
    plt.plot(sizes, y_gpu, '-.^g', markersize=7)
    plt.plot(sizes, y_cpu, '-.or', markersize=7)

    plt.grid(linestyle="dotted")

    plt.suptitle('{0} threads, using rdtsc'.format(num_threads), fontsize=14, fontweight='bold')

    plt.savefig('plot_{0}threads_ratio_O0.png'.format(num_threads))

vary_dataset_size()