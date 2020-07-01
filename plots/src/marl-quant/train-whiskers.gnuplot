set xlabel "BinSet"
set ylabel "Ratio Legit Traffic Preserved"

set style boxplot outliers pointtype 7
set style data boxplot

set boxwidth 1
unset key
set pointsize 0.1

file(n) = sprintf("dataset_%d.dat",n)

plot for [i=0:9] file(i) u (i * 1.0):3:(1.0) ps .1 notitle

# plot '../results/tnsm-ecmp-2-opus-m-separate.avg.csv' u (0.0):3:(1.0) ls 1 ps .1, \
#      '../results/tnsm-ecmp-2-opus-mpp-single.avg.csv' u (1.0):3:(1.0) ls 1 ps .1, \
#      '../results/tnsm-ecmp-2-opus-spf-single.avg.csv' u (2.0):3:(1.0) ls 1 ps .1, \
#      '../results/tnsm-ecmp-4-opus-m-separate.avg.csv' u (4.0):3:(1.0) ls 3 ps .1, \
#      '../results/tnsm-ecmp-4-opus-mpp-single.avg.csv' u (5.0):3:(1.0) ls 3 ps .1, \
#      '../results/tnsm-ecmp-4-opus-spf-single.avg.csv' u (6.0):3:(1.0) ls 3 ps .1, \
#      '../results/tnsm-ecmp-8-opus-m-separate.avg.csv' u (8.0):3:(1.0) ls 5 ps .1, \
#      '../results/tnsm-ecmp-8-opus-mpp-single.avg.csv' u (9.0):3:(1.0) ls 5 ps .1, \
#      '../results/tnsm-ecmp-8-opus-spf-single.avg.csv' u (10.0):3:(1.0) ls 5 ps .1, \
#      '../results/tnsm-ecmp-16-opus-m-separate.avg.csv' u (12.0):3:(1.0) ls 7 ps .1, \
#      '../results/tnsm-ecmp-16-opus-mpp-single.avg.csv' u (13.0):3:(1.0) ls 7 ps .1, \
#      '../results/tnsm-ecmp-16-opus-spf-single.avg.csv' u (14.0):3:(1.0) ls 7 ps .1, \
