set xlabel "BinSet"
set ylabel "Divergence"

#set yrange [0.15:0.3]

plot "../results/marl-quant/process-q-divs-split.csv" u 0:11:12 with yerrorlines notitle
