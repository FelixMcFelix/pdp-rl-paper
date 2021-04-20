load "gnuplot-palettes/inferno.pal"
set datafile separator ","

set xlabel "Cores allocated"
set ylabel "State$\\rightarrow$Action Latency (\\si{\\micro\\second})"

set key textcolor rgb "black"
set tics textcolor rgb "black"
set label textcolor rgb "black"

set key samplen 2 spacing 1 width -3 above

array BitDepths[3]
BitDepths[1] = 8
BitDepths[2] = 16
BitDepths[3] = 32

array LineStyles[6]
LineStyles[1] = 3
LineStyles[2] = 4
LineStyles[3] = 5
LineStyles[4] = 6
LineStyles[5] = 7
LineStyles[6] = 8

array DashStyles[6]
DashStyles[1] = 1
DashStyles[2] = 2
DashStyles[3] = 3
DashStyles[4] = 4
DashStyles[5] = 5
DashStyles[6] = 6

array Singles[3]
Singles[1] = 455.053
Singles[2] = 461.827
Singles[3] = 454.773

myTitle(i) = sprintf("Parallel (%d-bit)", BitDepths[i + 1])
singleTitle(i) = sprintf("Single (%d-bit)", BitDepths[i + 1])
file(i) = sprintf("../results/rl-perf-tester/vary-core-ct/%d/SUMMARY.balanced.ComputeAndWriteout.csv", BitDepths[i + 1])

plot for [i=0:2] file(i) u 1:(column(4)/1.2e3) every ::1 with linespoints title myTitle(i) ls LineStyles[i + 1] dt DashStyles[i + 1], \
	for [i=0:2] Singles[i + 1] ls LineStyles[i + 4] dt DashStyles[i + 4] title singleTitle(i)
