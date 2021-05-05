load "gnuplot-palettes/inferno.pal"
set datafile separator ","

set multiplot

set xlabel "Dimensions in Tiling"
set ylabel "Policy Update Time (\\si{\\micro\\second})"

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
file(n) = sprintf("../results/rl-perf-tester/vary-work-ct/%d/SUMMARY.%s.UpdateAll.csv", BitDepths[(i % 3) + 1], Names[(n / 3) + 1])

set style arrow 1 ls 102 backhead filled
set style arrow 2 ls 102 nohead nofilled

set arrow from 1.5, 22.5 to screen 0.25, screen 0.5 lw 1 as 1

set arrow arrowstyle 2

set arrow from 0.9,5 to 2.1,5 front nohead as 2 
set arrow from 0.9,5 to 0.9,19.5 front nohead as 2
set arrow from 2.1,19.5 to 0.9,19.5 front nohead as 2
set arrow from 2.1,19.5 to 2.1,5 front nohead as 2

# 2 types
# times 3 bit depths.
plot for [i=0:5] file(i) u 1:(column(4)/1.2e3) every ::1 with linespoints title myTitle(i) ls LineStyles[i + 1] dt DashStyles[i + 1]

#Multiplot time
set origin 0.16,0.32
set size 0.15,0.45
set bmargin 0; set tmargin 0; set lmargin 0; set rmargin 0
unset arrow
set border 15
clear

set nokey
set xlabel ""
set ylabel ""
unset xtics
unset ytics
unset grid
set xrange [0.9:2.1]
set yrange [5:19.5]

plot for [i=0:5] file(i) u 1:(column(4)/1.2e3) every ::1 with linespoints title myTitle(i) ls LineStyles[i + 1] dt DashStyles[i + 1]

