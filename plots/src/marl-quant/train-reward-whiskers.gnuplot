set xlabel "BinSet"
set ylabel "Ratio Legit Traffic Preserved"

set style boxplot outliers pointtype 7
set style data boxplot

set boxwidth 1
unset key
set pointsize 0.1

file(n) = sprintf("../results/marl-quant/quant-train-%d.avg.csv",n)

plot for [i=0:32] file(i) u (i * 1.0):2:(1.0) ps .1 notitle

