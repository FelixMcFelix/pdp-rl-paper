#!/bin/python3

import numpy as np
import pathlib
import subprocess

RESULTS_DIR = "../../../results/ring-comm-timing/"

RTSYM_CMD = "/opt/netronome/bin/nfp-rtsym"

locations = [
    "_me_me_times",
    "_isl_isl_times",
    "_nn_times",
    "_ref_times",
]

partners = [
    None,
    None,
    3,
    None,
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
    return np.array([np.mean(npd), np.var(npd), np.std(npd), np.median(npd), np.percentile(npd, 95), np.percentile(npd, 99)])

pathlib.Path(RESULTS_DIR).mkdir(parents=True, exist_ok=True)

indiv_measures = [measure_data(location) for location in locations]
cycle_measures = [process_ints(ind) for ind in indiv_measures]

scales = [("-cycles", 1.0), ("-ns", 1.0/1.2)]

for (scale_name, scale) in scales:
    for (name, measures) in zip(locations, cycle_measures):
        with open("{}{}{}.txt".format(RESULTS_DIR, name, scale_name), "w") as of:
            of.write(np.array2string(measures * scale))
            of.write("\n")

    for (name, measures, partner) in zip(locations, cycle_measures, partners):
        with open("{}{}-scaled{}.txt".format(RESULTS_DIR, name, scale_name), "w") as of:
            if partner is None:
                to_write = measures / 2.0
            else:
                m = measures
                p = cycle_measures[partner] / 2.0
                to_write = np.array([m[0] - p[0], m[1] + p[1], m[2] + p[2], m[3] - p[3]])

            of.write(np.array2string(to_write * scale))
            of.write("\n")
    
