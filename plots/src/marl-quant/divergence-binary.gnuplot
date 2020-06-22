set xlabel "BinSet"
set ylabel "Divergence"

plot "../results/marl-quant/process-q-divs.csv" matrix every ::1 with linespoint
