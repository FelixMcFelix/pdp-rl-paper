load "gnuplot-palettes/gnbu.pal"
set datafile separator ","

set ylabel "Probability"
set xlabel "State-Action Latency (\\si{\\micro\\second})"

set yrange [0:1]

set key textcolor rgb "black"
set tics textcolor rgb "black"
set label textcolor rgb "black"

set key samplen 2 spacing 1 width -3 above

array BitDepths[3]
BitDepths[1] = 8
BitDepths[2] = 16
BitDepths[3] = 32

array LineStyles[6]
LineStyles[1] = 8 
LineStyles[2] = 7
LineStyles[3] = 6
LineStyles[4] = 5
LineStyles[5] = 4
LineStyles[6] = 3

array DashStyles[6]
DashStyles[1] = 1
DashStyles[2] = 2
DashStyles[3] = 3
DashStyles[4] = 4
DashStyles[5] = 5
DashStyles[6] = 6

array Singles[3]
Singles[1] = 193.413
Singles[2] = 195.320
Singles[3] = 185.040

myTitle(i) = sprintf("Parallel (%d-bit)", BitDepths[i + 1])
singleTitle(i) = sprintf("Single (%d-bit)", BitDepths[i + 1])
file(i) = sprintf("../results/rl-perf-tester/vary-core-ct/%d/balanced.28d.31c.ComputeAndWriteout.dat", BitDepths[i + 1])
singleFile(i) = sprintf("../results/rl-perf-tester/vary-work-ct/%d/single.28d.31c.ComputeAndWriteout.dat", BitDepths[i + 1])

plot for [i=2:2] file(i) u (column(1)/1.2e3):(1./5000.) smooth cumul with lines title myTitle(i) ls LineStyles[1] dt DashStyles[1], \
	for [i=2:2] singleFile(i) u (column(1)/1.2e3):(1./5000.) smooth cumul with lines title singleTitle(i) ls LineStyles[2] dt DashStyles[2], \
	"../results/host-rl-perf-tput/f32-opt-lats.dat" u (column(1)):(1./21664.) smooth cumul with lines title "Float32" ls LineStyles[4] dt DashStyles[3], \
	"../results/host-rl-perf-tput/f32-overload-lats.dat" u (column(1)):(1./20997.) smooth cumul with lines title "Float32 (Over)" ls LineStyles[5] dt DashStyles[4]

