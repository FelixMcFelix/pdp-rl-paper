load "gnuplot-palettes/inferno.pal"
set datafile separator ","

set xlabel "Dimensions in Tiling"
set ylabel "State$\\rightarrow$Action Latency (\\si{\\micro\\second})"

set key textcolor rgb "black"
set tics textcolor rgb "black"
set label textcolor rgb "black"

set key samplen 2 spacing 1 width -3 above

array Names[2]
Names[1] = "balanced"
Names[2] = "single"

array PresentationNames[2]
PresentationNames[1] = "Parallel"
PresentationNames[2] = "Single"

array LineStyles[2]
LineStyles[1] = 3
LineStyles[2] = 7

array DashStyles[2]
DashStyles[1] = 1
DashStyles[2] = 2

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

myTitle(i) = sprintf("%s (%d-bit)", PresentationNames[(i / 3) + 1], BitDepths[(i % 3) + 1])
file(n) = sprintf("../results/rl-perf-tester/vary-work-ct/%d/SUMMARY.%s.ComputeAndWriteout.csv", BitDepths[(i % 3) + 1], Names[(n / 3) + 1])

# 2 types
# times 3 bit depths.
plot for [i=0:5] file(i) u 1:(column(4)/1.2e3) every ::1 with linespoints title myTitle(i) ls LineStyles[i + 1] dt DashStyles[i + 1]
