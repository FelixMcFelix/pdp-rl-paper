import numpy as np
import scipy.stats as spss

files_dir = "raw/"
const_secs = 10.0

bitdepths = ["i8", "i16", "i32", "f32"]
machines = [
	"perlman",
	"beast2",
	"beast1",
]
act_modes = ["ComputeAndWriteout", "UpdateAll"]
trials = 10
thread_max = 32

for bd in bitdepths:
	for machine in machines:
		for act_mode in act_modes:
			print("Working on {}-{}-{}".format(bd, machine, act_mode))
			# out format: mean_tput, std_tput, med_time, 99_time, 9999_time, mean_time, std_time
			expt_rows = []

			last_all_tputs = None

			for thread_ct in range(1, thread_max+1):
				print("\t{} threads".format(thread_ct))
				all_tputs = []
				all_items = []

				for trial_i in range(trials):
					items = []
					for thread_i in range(thread_ct):
						fname = "{}{}.{}.{}.{}({}).{}.dat".format(
							files_dir,
							bd,
							machine,
							act_mode,
							thread_ct,
							thread_i,
							trial_i,
						)
						with open(fname, "r") as of:
							for line in of:
								items.append(float(line.strip()))

					all_tputs.append(float(len(items))/const_secs)
					all_items += items

				if last_all_tputs is None:
					p_value = 0.0
				else:
					(_t_val, p_value) = spss.stats.ttest_ind(all_tputs, last_all_tputs, equal_var=False)

				expt_rows.append([
					np.mean(all_tputs),
					np.std(all_tputs),
					np.median(all_items),
					np.percentile(all_items, 99.0),
					np.percentile(all_items, 99.99),
					np.mean(all_items),
					np.std(all_items),
					p_value,
				])

				last_all_tputs = all_tputs

			# writeout
			outfile = "SUMMARY.{}.{}.{}.dat".format(bd, machine, act_mode)
			with open(outfile, "w") as of:
				for row in expt_rows:
					format_row = ["{:.3f}".format(x) for x in row]
					of.write(" ".join(format_row) + "\n")
