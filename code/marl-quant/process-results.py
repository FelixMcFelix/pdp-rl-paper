import csv
import numpy as np

results_dir = "../../results/marl-quant/"

divergence_file_name = "quant-results.csv"
divergence_file = results_dir + divergence_file_name

# note: must needs be formatted.
out_file_name = "process-q-{}.csv"
out_file = results_dir + out_file_name

def decision_correct(good, state, choice):
    # if bad, always want to see 2 (state 5 allows 0)
    # if good, always want to see 1 (state 0 allows 0)
    allow_stay = (good and state == 0) or state == 5
    return (choice == 0 and allow_stay) or (good and choice == 1) or (not good and choice == 2)

def update_accs(array, boolset, count):
    contrib = [(1.0 if a else 0.0) for a in boolset]
    return array + ((contrib - array)/ float(count + 1))

NUM_LINES_PER_EXPT = 1000
accuracy = []
accuracies = []
div = []
divs = []
with open(divergence_file, "r") as of:
    r = csv.reader(of)
    total_measures = 0
    measures_in_expt = 0
    for i, row in enumerate(r):
        expt_num = i // NUM_LINES_PER_EXPT

        if len(accuracies) <= expt_num:
            accuracies.append([])
            divs.append([])
            measures_in_expt = 0

        if len(row) > 3:
            good = row[0] == "True"
            state = int(row[1])
            decisions = [int(v) for v in row[2:]]
            corrects = [decision_correct(good, state, d) for d in decisions]
            diverged = [decisions[0] == v for v in decisions[1:]]

            if len(accuracy) == 0:
                accuracy = np.zeros(len(corrects))
                div = np.zeros(len(diverged))

            if len(accuracies[expt_num]) == 0:
                accuracies[expt_num] = np.zeros(len(corrects))
                divs[expt_num] = np.zeros(len(diverged))

            accuracy = update_accs(accuracy, corrects, total_measures)
            accuracies[expt_num] = update_accs(accuracies[expt_num], corrects, measures_in_expt)
            div = update_accs(div, diverged, total_measures)
            divs[expt_num] = update_accs(divs[expt_num], diverged, measures_in_expt)

            measures_in_expt += 1
            total_measures += 1

targets = {
    "accs": [accuracy],
    "accs-split": accuracies,
    "divs": [div],
    "divs-split": divs,
}

for name, rowset in targets.items():
    true_name = out_file.format(name)
    with open(true_name, "w") as of:
        wr = csv.writer(of)
        for row in rowset:
            wr.writerow(row)

