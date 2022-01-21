import numpy as np
import sys

sys.stdout.reconfigure(encoding='utf-8')

# sub in: bit depth, fw-type, measure_name
float_results_fmt_str = "../results/host-rl-perf-tput/SUMMARY.f32.{}.{}.dat"
host_results_fmt_str = "../results/host-rl-perf-tput/SUMMARY.{}.{}.{}.dat"
results_fmt_str = "../results/rl-perf-tester/vary-work-ct/{}/{}.28d.31c.{}.dat"

cycle_in_ns = 1e9 / 1.2e9

core_ct_for_tput = 32
types = [("single", R"\Indfw{}", True), ("balanced", R"\Coopfw{}", False)]
measures = [("ComputeAndWriteout", R"\xmark", False),("UpdateAll", R"\cmark", True)]
bitdepths = [32, 16, 8]

print(R"\begin{tabular}{@{}ccS[table-format=4.3]S[table-format=4.3]S[table-format=4.3]S[table-format=4.3]S[table-format=4.3]S[table-format=4.3]@{}}")
print(R"\toprule\multicolumn{1}{c}{Datatype} & \multicolumn{1}{c}{Machine/FW} & \multicolumn{3}{c}{State-Action Latency (\si{\micro\second})} & \multicolumn{3}{c}{State-Update Time (\si{\micro\second})}\\")

# Just generate the next header line...
t2_header = R"\cmidrule(lr){3-5}\cmidrule(lr){6-8} &"

measure_names = ["Median", R"\nth{99}", R"\num{99.99}\nthscript{th}"]

for i in range(2):
	for name in measure_names:
		t2_header += R" & \multicolumn{1}{c}{"+name+R"}"	

t2_header += R"\\ \midrule"

best_row_for_each_column = [9, 9, 9, 9, 9, 9]
curr_row = 0

print(t2_header)

# Deal with Float results here.
# File names like: results/host-rl-perf-tput/SUMMARY.f32.beast1.ComputeAndWriteout.dat
# These results are already in micros.

machines = [("Collector", "beast2", 4), ("MidServer", "perlman", 6)]
dtypes = [("f32", "Float"), ("i32", "NpInt32"), ("i16", "NpInt16"), ("i8", "NpInt8")]

for j, (dtype_str, dtype_format) in enumerate(dtypes):
	for i, (machine_pres, machine_name, core_limit) in enumerate(machines):
		out = "{} & {}".format(dtype_format if i == 0 else "", machine_pres)

		latencies = []

		for online_i, (online_fname_part, online_latex_part, needs_lock) in enumerate(measures):
			fname = host_results_fmt_str.format(dtype_str, machine_name, online_fname_part)

			best_core_ct = 0
			best_dats = None
			# need to find the "best line"
			with open(fname) as of:
				lines = of.readlines()
				for line_i, line in enumerate(lines):
					floats = [float(x) for x in line.split(" ")]

					if line_i < core_limit and (best_dats is None or (floats[-1] < 0.05 and floats[0] > best_dats[0])):
						best_dats = floats
						best_core_ct = line_i
						# print(line_i, "was the winner")
					elif best_dats is not None:
						break

			latencies += best_dats[2:5]

		for os in latencies:
			out += " & {}".format(os)

		out += R"\\"

		print(out)
		curr_row += 1

print(R"\cmidrule(lr){1-8}")

for bits in bitdepths:
	for type_i, (type_fname_part, pres, do_div) in enumerate(types):
		out = "{} & {}".format("Int{}".format(bits) if type_i == 0 else "", R"\approachshort{}-"+pres)

		latencies = []

		# Throughput
		# for type_fname_part, _p, do_div in types:
		for online_i, (online_fname_part, online_latex_part, needs_lock) in enumerate(measures):
			fname = results_fmt_str.format(bits, type_fname_part, online_fname_part)
			indiv_data = []
			with open(fname) as of:
				lines = of.readlines()
				indiv_data = np.array([float(x.strip()) for x in lines])
			indiv_data *= cycle_in_ns;
			indiv_data /= 1.0e3;

			stats = [
				np.median(indiv_data),
				np.percentile(indiv_data, 99.0),
				np.percentile(indiv_data, 99.99),
			]

			latencies += ["{:.3f}".format(x) for x in stats]

		for column, row in enumerate(best_row_for_each_column):
			if row == curr_row:
				latencies[column] = "\\bfseries {}".format(latencies[column])

		for os in latencies:
			out += " & {}".format(os)

		out += R"\\"

		print(out)
		curr_row += 1

print(R"\bottomrule")
print(R"\end{tabular}")
