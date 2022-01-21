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
measures = [("ComputeAndWriteout", "Offline", False),("UpdateAll", "Online", True)]
bitdepths = [32, 16, 8]

print(R"\begin{tabular}{@{}ccS[table-format=2]S[table-format=3.3]S[table-format=2.3]S[table-format=1.3]S[table-format=1.3]@{}}")
print(R"\toprule\multicolumn{1}{c}{Datatype} & \multicolumn{1}{c}{Machine/FW} & \multicolumn{1}{c}{Workers} & \multicolumn{2}{c}{Throughput (k actions/s)} & \multicolumn{2}{c}{Throughput/core (k actions/s)}\\")

# Just generate the next header line...
t2_header = R"\cmidrule(lr){4-5}\cmidrule(lr){6-7} & &"

for i in range(2):
	for (_f, name, _n) in measures:
		t2_header += R" & \multicolumn{1}{c}{"+name+R"}"	

t2_header += R"\\ \midrule"

print(t2_header)

best_row_for_each_column = [8, 9, 8, 9]
curr_row = 0

# Deal with Float results here.
# File names like: results/host-rl-perf-tput/SUMMARY.f32.beast1.ComputeAndWriteout.dat
# These results are already in micros.

machines = [("Collector", "beast2", 4), ("MidServer", "perlman", 6)]
dtypes = [("f32", "Float"), ("i32", "NpInt32"), ("i16", "NpInt16"), ("i8", "NpInt8")]

for j, (dtype_str, dtype_format) in enumerate(dtypes):
	for i, (machine_pres, machine_name, core_limit) in enumerate(machines):
		out = "{} & {}".format(dtype_format if i == 0 else "", machine_pres)

		best_core = None
		tputs = []
		stds = []
		core_cts = []

		for online_i, (online_fname_part, online_latex_part, needs_lock) in enumerate(measures):
			fname = host_results_fmt_str.format(dtype_str, machine_name, online_fname_part)

			best_core_ct = 0
			best_dats = None
			# need to find the "best line"
			with open(fname) as of:
				lines = of.readlines()
				if needs_lock:
					# take line 0, baybee
					best_dats = [float(x) for x in lines[0].split(" ")]
				elif best_core is None:
					for line_i, line in enumerate(lines):
						floats = [float(x) for x in line.split(" ")]

						if line_i < core_limit and (best_dats is None or (floats[-1] < 0.05 and floats[0] > best_dats[0])):
							best_dats = floats
							best_core_ct = line_i
							best_core = best_core_ct
							# print(line_i, "was the winner")
						elif best_dats is not None:
							break
				else:
					#take line best_core
					best_dats = [float(x) for x in lines[best_core].split(" ")]

			tputs.append(best_dats[0])
			stds.append(best_dats[1])
			core_cts.append(float(best_core + 1) if not needs_lock else 1.0)

		out += " & {}".format(best_core + 1)

		tputs = np.array(tputs) / 1000.0
		stds = np.array(stds) / 1000.0

		scaled_down_tputs = np.array(tputs) / np.array(core_cts)
		scaled_down_stds = np.array(stds) / np.array(core_cts)

		formatted = ["{:.3f} \\pm {:.3f}".format(t, s) for (t, s) in zip(tputs, stds)]
		formatted += ["{:.3f} \\pm {:.3f}".format(t, s) for (t, s) in zip(scaled_down_tputs, scaled_down_stds)]

		formatted[-1] = R"\multicolumn{1}{c}{---}"

		for os in formatted:
			out += " & {}".format(os)

		out += R"\\"

		print(out)
		curr_row +=1

print(R"\cmidrule(lr){1-7}")

for bits in bitdepths:
	for type_i, (type_fname_part, pres, do_div) in enumerate(types):
		out = "{} & {}".format("Int{}".format(bits) if type_i == 0 else "", R"\approachshort{}-"+pres)

		tputs = []
		stds = []
		core_cts = []

		formatted = []

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

			indiv_tputs = 1e6 / indiv_data

			if do_div and not needs_lock:
				# tput_parts[0] = tput_parts[0] * core_ct_for_tput
				indiv_tputs *= float(core_ct_for_tput)

			stats = [
				np.median(indiv_data),
				np.percentile(indiv_data, 99.0),
				np.percentile(indiv_data, 99.99),
			]

			tputs.append(np.mean(indiv_tputs))
			stds.append(np.std(indiv_tputs))
			core_cts.append(float(core_ct_for_tput))

		tputs = np.array(tputs) / 1000.0
		stds = np.array(stds) / 1000.0

		# print(core_cts)

		scaled_down_tputs = np.array(tputs) / np.array(core_cts)
		scaled_down_stds = np.array(stds) / np.array(core_cts)

		formatted = ["{:.3f} \\pm {:.3f}".format(t, s) for (t, s) in zip(tputs, stds)]
		formatted += ["{:.3f} \\pm {:.3f}".format(t, s) for (t, s) in zip(scaled_down_tputs, scaled_down_stds)]

		if do_div:
			formatted[-1] = R"\multicolumn{1}{c}{---}"

		for column, row in enumerate(best_row_for_each_column):
			if row == curr_row:
				formatted[column] = "\\bfseries {}".format(formatted[column])

		out += " & {}".format(core_ct_for_tput)

		for os in formatted:
			out += " & {}".format(os)

		out += R"\\"

		print(out)
		curr_row += 1

print(R"\bottomrule")
print(R"\end{tabular}")
