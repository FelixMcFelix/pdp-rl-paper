load "gnuplot-palettes/inferno.pal"
set datafile separator ","

set xlabel "Cores allocated"
set ylabel "Time (\si{\micro\second})"

set key top left

array Names[2]
Names[1] = "balanced"
# Names[2] = "single"

array PresentationNames[2]
PresentationNames[1] = "Parallel"
# PresentationNames[2] = "Single Core"

array Variants[2]
Variants[1] = "Median"
Variants[2] = "99th Percentile"

array TargetColumn[2]
TargetColumn[1] = 4
TargetColumn[2] = 5

array LineStyles[2]
LineStyles[1] = 3
LineStyles[2] = 7

array DashStyles[2]
DashStyles[1] = 1
DashStyles[2] = 2

myTitle(i) = sprintf("%s (%s)", PresentationNames[(i / 2) + 1], Variants[(i % 2) + 1])
file(n) = sprintf("../results/rl-perf-tester/vary-core-ct/16/SUMMARY.%s.UpdateAll.csv", Names[(n / 2) + 1])

plot for [i=0:1] file(i) u 1:(column(TargetColumn[(i % 2) + 1])/1.2e3) every ::1 with linespoints title myTitle(i) ls LineStyles[(i / 2) + 1] dt DashStyles[(i % 2) + 1]
