set xlabel "Rule Count"
set ylabel "Time (ms)"

set style boxplot outliers pointtype 7
set style data boxplot

set boxwidth 0.5
unset key
set pointsize 0.1

set xrange [1:65537]

set logscale x

file(n) = sprintf("../results/rte-timer/%d.csv",1<<n)

plot for [i=0:16] file(i) u (1<<i):($1/1e6):(1.0) ps .1 notitle

