set xlabel "Rule Count"
set ylabel "Time (ns)"

set style boxplot outliers pointtype 7
set style data boxplot

set boxwidth 1
unset key
set pointsize 0.1

file(n) = sprintf("../results/rte-timer/%d.csv",1<<n)

plot for [i=0:16] file(i) u (1 << i):1:(1.0) ps .1 notitle

