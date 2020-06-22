set xlabel "BinSet"
set ylabel "Divergence"

plot for [i=1:10] '../results/marl-quant/process-q-divs-split.csv' u i with linespoint notitle

