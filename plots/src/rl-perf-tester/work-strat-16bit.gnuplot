# set xlabel "Rule Count"
set ylabel "Time (us)"

#set yrange [0:130]

set style boxplot nooutliers pointtype 7
set style data boxplot

set boxwidth 0.5
unset key
set pointsize 0.1

array Names[5]
Names[1] = "chunk"
Names[2] = "randomised"
Names[3] = "stride"
Names[4] = "offset_stride"
Names[5] = "balanced"

array Stages[4]
Stages[1] = "Compute"
Stages[2] = "ComputeAndWriteout"
Stages[3] = "UpdateOnlyPrep"
Stages[4] = "UpdateAll"

array LineStyles[5]
LineStyles[1] = 1
LineStyles[2] = 2
LineStyles[3] = 4
LineStyles[4] = 6
LineStyles[5] = 7

file(n) = sprintf("../results/rl-perf-tester/work-strats/16/%s.28d.31c.%s.dat", Names[(n / 4) + 1], Stages[(n % 4) + 1])
x_coord(n) = 1 + floor(n / 4) + n

plot for [i=0:19] file(i) u (x_coord(i)):($1/1.2e3):(1.0) notitle ls LineStyles[(i % 5) + 1]
