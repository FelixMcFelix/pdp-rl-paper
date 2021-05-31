import math
import numpy as np
import scipy.stats as spss

stress_result_dir = "stress/"
out_dir = "stress-process/"

# pkt sz, stress level, iter
stress_file_lat = "l-{}B-{}k-{}.dat"
stress_file_tpu = "t-{}B-{}k-{}.dat"

histo_bin_width = 200

pkt_sizes = [64, 128, 256, 512, 1024, 1280, 1518];
iter_ct = 10
rate_max = 16 + 1

def write_matrix(name, matrix, w, h):
	# these go in by rates, then pkt_szs
	# need to transpose
	npd = np.array(matrix)
	shaped = np.reshape(npd, [w, h])
	shaped = shaped.T

	print(shaped)

	with open(out_dir + name + ".dat", "w") as of:
		for x, row in enumerate(shaped):
			#if not x == 0:
			#	of.write("\n")

			#for y, val in enumerate(row):
			#	of.write("{} {} {}\n".format(x, y, val))
			of.write(" ".join([str(x) for x in row]) + "\n")

	rel = shaped - shaped[0]
	with open(out_dir + name + "rel.dat", "w") as of:
		for x, row in enumerate(rel):
			#if not x == 0:
			#	of.write("\n")

			#for y, val in enumerate(row):
			#	of.write("{} {} {}\n".format(x, y, val))
			of.write(" ".join([str(x) for x in row]) + "\n")

	perc = ((shaped / shaped[0]) - 1.0) * 100.0
	with open(out_dir + name + "perc.dat", "w") as of:
		for x, row in enumerate(perc):
			#if not x == 0:
			#	of.write("\n")

			#for y, val in enumerate(row):
			#	of.write("{} {} {}\n".format(x, y, val))
			of.write(" ".join([str(x) for x in row]) + "\n")

print("Latencies!")

matrices = {}

def add_metric(name, metric):
	if name not in matrices:
		matrices[name] = []

	matrices[name].append(metric)

# process latencies first.
for pkt_size in pkt_sizes:
	lats_baseline = None
	last_over_base = None
	prev = None
	for rate in range(rate_max):
		lats_for_this_cell = []
		for i in range(iter_ct):
			with open(stress_result_dir + stress_file_lat.format(pkt_size, rate, i)) as of:
				next(of)
				lats_for_this_cell += [float(x.strip()) for x in of]

		p_val = None
		if lats_baseline is None:
			lats_baseline = lats_for_this_cell
			p_val = 1.0
		else:
			# read this as "checking that baseline is less than new"
			(_t_val, p_val) = spss.stats.mannwhitneyu(lats_baseline, lats_for_this_cell, alternative='less')

		if last_over_base is not None:
			versus_prev = spss.stats.mannwhitneyu(last_over_base[1], lats_for_this_cell, alternative='less')[1]
			if versus_prev < 0.05:
				print("{}k worse than {}k with P_H0={}".format(rate, last_over_base[0], versus_prev))
				last_over_base = (rate, lats_for_this_cell)
		else:
			last_over_base = (rate, lats_for_this_cell)

		if prev is not None:
			versus_prev = spss.stats.mannwhitneyu(prev, lats_for_this_cell, alternative='two-sided')[1]
			if versus_prev >= 0.05:
				print("{}k same-ish as {}k".format(rate, rate-1))
		prev = lats_for_this_cell

		metrics = [
			np.median(lats_for_this_cell),
			np.percentile(lats_for_this_cell, 99.0),
			np.percentile(lats_for_this_cell, 99.99),
			np.max(lats_for_this_cell),
			np.mean(lats_for_this_cell),
			np.std(lats_for_this_cell),
			p_val,
		]

		base_names = ["median", "two-9", "four-9", "max", "mean", "std", "p"]

		for (name, metric) in zip(base_names, metrics):
			add_metric("matrix-latency-"+name, metric)

		print("{}B {}k: {} (skew {}, kurtosis {})".format(pkt_size, rate, metrics, spss.skew(lats_for_this_cell), spss.kurtosis(lats_for_this_cell)))

		with open(out_dir + "l-{}B-{}k.dat".format(pkt_size, rate), "w") as of:
			for lat in lats_for_this_cell:
				of.write("{}\n".format(int(lat)))

		with open(out_dir + "hl-{}B-{}k.dat".format(pkt_size, rate), "w") as of:
			max_lat = np.max(lats_for_this_cell)
			min_lat = np.min(lats_for_this_cell)

			max_bin = (math.ceil(max_lat / histo_bin_width) + 2) * histo_bin_width
			min_bin = math.floor(min_lat / histo_bin_width) * histo_bin_width

			bins = np.arange(min_bin, max_bin, histo_bin_width)

			histo = np.histogram(lats_for_this_cell, bins)

			for (bin_x, freq) in zip(bins, histo[0]):
				of.write("{},{}\n".format(bin_x / 1000.0, int(freq)))

for key, value in matrices.items():
	write_matrix(key, value, len(pkt_sizes), rate_max)

# { {
#     ibytes = 21536748776,
#     ierrors = 0,
#     imissed = 123196123,
#     ipackets = 21116998,
#     obytes = 147196761216,
#     oerrors = 0,
#     opackets = 144313056,
#     rx_nombuf = 0
#   },
#   n = 1
# }

def extract_from_line(line):
	return float(line.split(",")[0].split("=")[1].strip())

print("Loss rates!")

# process mean losses?
for pkt_size in pkt_sizes:
	losses_baseline = None
	for rate in range(rate_max):
		losses_for_this_cell = []
		for i in range(iter_ct):
			with open(stress_result_dir + stress_file_tpu.format(pkt_size, rate, i)) as of:
				lines = of.readlines()
				imissed = extract_from_line(lines[3])
				ipackets = extract_from_line(lines[4])
				opackets = extract_from_line(lines[7])

				made_it_to_nic = imissed + ipackets

				losses_for_this_cell += [1.0 - (made_it_to_nic / opackets)]

		p_val = None
		if losses_baseline is None:
			losses_baseline = losses_for_this_cell
		else:
			(_t_val, p_val) = spss.stats.ttest_ind(losses_baseline, losses_for_this_cell, equal_var=False)

		metrics = [
			np.median(losses_for_this_cell),
			np.percentile(losses_for_this_cell, 99.0),
			np.percentile(losses_for_this_cell, 99.99),
			np.mean(losses_for_this_cell),
			np.std(losses_for_this_cell),
			p_val,
		]
		print("{}B {}k: {}".format(pkt_size, rate, metrics))
