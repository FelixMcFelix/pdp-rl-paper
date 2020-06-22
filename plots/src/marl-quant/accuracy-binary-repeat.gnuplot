set xlabel "BinSet"
set ylabel "Accuracy"

plot for [i=1:10] "../results/marl-quant/process-q-accs-split.csv" u i with linespoint notitle

