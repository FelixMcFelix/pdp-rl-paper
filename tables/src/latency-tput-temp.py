import numpy as np
import sys

sys.stdout.reconfigure(encoding='utf-8')

# sub in: bit depth, fw-type, measure_name
results_fmt_str = "../results/rl-perf-tester/vary-{}-ct/{}/{}.28d.31c.{}.dat"

cycle_in_ns = 1e9 / 1.2e9

core_ct_for_tput = 32
types = [("single", "work", "Single", True), ("balanced", "core", "Parallel", False)]
measures = [("UpdateAll", R"\cmark", True), ("ComputeAndWriteout", R"\xmark", False)]
bitdepths = [32, 16, 8]

print(R"\begin{tabular}{@{}ccSSSSSS@{}}")
print(R"\toprule\multicolumn{1}{c}{Bits} & \multicolumn{1}{c}{Online} & \multicolumn{2}{c}{Throughput (k actions/s)} & \multicolumn{4}{c}{Completion (\si{\micro\second})}\\")

# Just generate the next header line...
t2_header = R"\cmidrule(lr){3-4}\cmidrule(lr){5-8} &"

for i in range(2):
	for (fname_part, w_core, pres_part, div_by_core_ct) in types:
		parts = [pres_part]
		if i != 0:
			parts.append(pres_part + R" (\nth{99})")

		for part in parts:
			t2_header += R" & \multicolumn{1}{c}{"+part+R"}"

t2_header += R"\\ \midrule"

print(t2_header)

for bits in bitdepths:
	for online_i, (online_fname_part, online_latex_part, needs_lock) in enumerate(measures):
		out = "{} & {}".format(bits if online_i == 0 else "", online_latex_part)

		latencies = []
		tputs = []

		# Throughput
		for type_fname_part, w_core, _p, do_div in types:
			fname = results_fmt_str.format(w_core, bits, type_fname_part, online_fname_part)
			indiv_data = []
			with open(fname) as of:
				lines = of.readlines()
				indiv_data = np.array([float(x.strip()) for x in lines])
			indiv_data *= cycle_in_ns;
			indiv_data /= 1.0e3;

			stats = [
				np.median(indiv_data),
				np.percentile(indiv_data, 99),
			]

			latencies.append("{:.3f}".format(stats[0]))
			latencies.append("{:.3f}".format(stats[1]))

			# convert indiv_data to t_puts.
			indiv_tputs = 1e6 / indiv_data

			if do_div and not needs_lock:
				# tput_parts[0] = tput_parts[0] * core_ct_for_tput
				indiv_tputs *= float(core_ct_for_tput)

			indiv_tputs /= 1.0e3

			tputs.append("{:.3f} \\pm {:.3f}".format(np.mean(indiv_tputs), np.var(indiv_tputs)))

		for os in tputs + latencies:
			out += " & {}".format(os)

		out += R"\\"

		print(out)

print(R"\bottomrule")
print(R"\end{tabular}")
