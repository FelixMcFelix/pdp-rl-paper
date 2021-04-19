load "gnuplot-palettes/inferno.pal"

# set xlabel "Rule Count"
set ylabel "Time (\\si{\\micro\\second})"

#set yrange [0:130]

set style boxplot nooutliers pointtype 7
set style data boxplot

set boxwidth 0.5
unset key
set pointsize 0.1

set key textcolor rgb "black"
set tics textcolor rgb "black"
set label textcolor rgb "black"

set key samplen 2 spacing 1 width -3

array Names[5]
Names[1] = "chunk"
Names[2] = "randomised"
Names[3] = "stride"
Names[4] = "balanced"

array PNames[5]
PNames[1] = "Na\\\"{i}ve"
PNames[2] = "Random"
PNames[3] = "Modular"
PNames[4] = "Balanced"

array Stages[3]
Stages[1] = "ComputeAndWriteout"
Stages[2] = "UpdateOnlyPrep"
Stages[3] = "UpdateAll"

array PStages[3]
PStages[1] = "Action"
PStages[2] = "Update Prep"
PStages[3] = "Update"

array LineStyles[5]
LineStyles[1] = 1
LineStyles[2] = 2
LineStyles[3] = 4
LineStyles[4] = 6
LineStyles[5] = 7

file(n) = sprintf("../results/rl-perf-tester/work-strats/16/%s.28d.31c.%s.dat", Names[(n / 3) + 1], Stages[(n % 3) + 1])
x_coord(n) = 1 + (2*floor(n / 3)) + n

set xtics () # clear all tics
set xtics nomirror
set grid noxtics
set for [i=0:3] xtics add (PNames[i + 1] x_coord(3*i)+1)

set key above

plot for [i=0:11] file(i) u (x_coord(i)):($1/1.2e3):(1.0) notitle ls LineStyles[(i % 3) + 2], \
	for [i=0:2] NaN ls LineStyles[(i%3)+2] title PStages[(i%3)+1]
