#!/bin/python3

import numpy as np
import pathlib
import subprocess

RESULTS_DIR = "../../../results/ring-comm-timing/"

RTSYM_CMD = "/opt/netronome/bin/nfp-rtsym"

locations = [
    "_write_space",
]

def measure_data(location):
    text_data = subprocess.check_output([RTSYM_CMD, "-v", location], universal_newlines=True)
    return process_data(text_data)

def process_data(string_data):
    lines = string_data.splitlines()
    out = []
    for line in lines:
        u32s = [int(word, 0) for word in line.split()[1:]]
        out += u32s
    return out

def process_ints(measurements):
    npd = np.array(measurements, dtype=np.double)
    # 16 cycles per count.
    npd = npd * 16.0
    return npd#.array([np.mean(npd), np.var(npd), np.std(npd), np.median(npd), np.percentile(npd, 95), np.percentile(npd, 99)])

pathlib.Path(RESULTS_DIR).mkdir(parents=True, exist_ok=True)

indiv_measures = [measure_data(location) for location in locations]
cycle_measures = [process_ints(ind) for ind in indiv_measures]

cycle_measures = cycle_measures[0]

first_t1 = 1
first_t2 = first_t1 + 8 * 7
first_t3 = first_t2 + 8 * 8
last_t3  = first_t3 + 8

print("Mean Bias: {}".format(cycle_measures[:first_t1]))
print("Mean T1  : {}".format(np.mean(cycle_measures[first_t1:first_t2])))
print("Mean T2  : {}".format(np.mean(cycle_measures[first_t2:first_t3])))
print("Mean T3  : {}".format(np.mean(cycle_measures[first_t3:last_t3])))

