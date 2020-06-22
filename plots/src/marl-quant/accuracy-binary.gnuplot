set xlabel "BinSet"
set ylabel "Accuracy"

plot "../results/marl-quant/process-q-accs.csv" matrix every ::1 with linespoint
