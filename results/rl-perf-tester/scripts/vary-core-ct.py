import csv
import numpy as np

# bd, fw, cores, measure
folder_format = "vary-core-ct/{}/{}.28d.{}c.{}.dat"
# bd, fw, measure
out_format = "vary-core-ct/{}/SUMMARY.{}.{}.csv"

bits = [8, 16, 32]
fws = ["randomised", "single"]
measures = ["UpdateAll", "ComputeAndWriteout"]
core_counts = range(1, 32)

for bit in bits:
	for fw in fws:
		for measure in measures:
			writeout_dir = out_format.format(bit, fw, measure)
			rows = []

			for d_count in core_counts:
				readin_dir = folder_format.format(bit, fw, d_count, measure)
				indiv_data = []
				with open(readin_dir) as of:
					lines = of.readlines()
					indiv_data = np.array([float(x.strip()) for x in lines])

				stats = [
					np.mean(indiv_data), np.std(indiv_data),
					np.median(indiv_data),
					np.percentile(indiv_data, 99),
					np.percentile(indiv_data, 75),
					np.percentile(indiv_data, 25),
				]

				rows.append([d_count] + ["{:.4f}".format(s) for s in stats])

			with open(writeout_dir, "w+") as of:
				writer = csv.writer(of, quoting=csv.QUOTE_NONE)
				writer.writerow(["cores", "mean_t", "std_t", "med_t", "99_t", "75_t", "25_t"])
				writer.writerows(rows)
