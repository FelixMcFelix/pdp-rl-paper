import numpy as np
import scipy.stats as spss

stress_result_dir = "stress/"

# pkt sz, stress level, iter
stress_file_lat = "l-{}B-{}k-{}.dat"
stress_file_tpu = "t-{}B-{}k-{}.dat"

pkt_sizes = [64, 128, 256, 512, 1024, 1280, 1518];
iter_ct = 10
rate_max = 16 + 1

def write_matrix(name, matrix):
	pass

print("Latencies!")

# process latencies first.
for pkt_size in pkt_sizes:
	lats_baseline = None
	for rate in range(rate_max):
		lats_for_this_cell = []
		for i in range(iter_ct):
			with open(stress_result_dir + stress_file_lat.format(pkt_size, rate, i)) as of:
				next(of)
				lats_for_this_cell += [float(x.strip()) for x in of]

		p_val = None
		if lats_baseline is None:
			lats_baseline = lats_for_this_cell
		else:
			(_t_val, p_val) = spss.stats.ttest_ind(lats_baseline, lats_for_this_cell, equal_var=False)

		metrics = [
			np.median(lats_for_this_cell),
			np.percentile(lats_for_this_cell, 99.0),
			np.percentile(lats_for_this_cell, 99.99),
			np.mean(lats_for_this_cell),
			np.std(lats_for_this_cell),
			p_val,
		]
		print("{}B {}k: {}".format(pkt_size, rate, metrics))

# { {
#     ibytes = 28044446236,
#     ierrors = 0,
#     imissed = 36943107,
#     ipackets = 226164889,
#     obytes = 32625360380,
#     oerrors = 0,
#     opackets = 263107745,
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
