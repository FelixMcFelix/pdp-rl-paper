set terminal tikz standalone color size 9cm,6cm font '\Large' preamble '\usepackage{microtype} \usepackage{libertinus} \usepackage{libertinust1math} \usepackage[binary-units, per-mode=symbol]{siunitx}\sisetup{detect-all} \newcommand{\approachshort}{OPaL} \newcommand{\Coopfw}{CoOp} \newcommand{\coopfw}{\Coopfw} \newcommand{\Indfw}{Ind} \newcommand{\indfw}{\Indfw}'

load "gnuplot-palettes/gnbu.pal"
set datafile separator ","

set ylabel "Cumulative Frequency"
set xlabel "State-Action Latency (\\si{\\micro\\second})"

set key textcolor rgb "black"
set tics textcolor rgb "black"
set label textcolor rgb "black"

set yrange [0:1]

#set key samplen 2 spacing 1 width -3 above
set key above width 3

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
DashStyles[2] = 5
DashStyles[3] = 3
DashStyles[4] = 4
DashStyles[5] = 5
DashStyles[6] = 6

array Singles[3]
Singles[1] = 193.413
Singles[2] = 195.320
Singles[3] = 185.040

myTitle(i) = sprintf("\\SI{%d}{\\bit}", BitDepths[i + 1])
singleTitle(i) = sprintf("\\emph{\\Indfw} (\\SI{%d}{\\bit})", BitDepths[i + 1])
file(i) = sprintf("../results/rl-perf-tester/vary-core-ct/%d/balanced.28d.31c.ComputeAndWriteout.dat", BitDepths[i + 1])
singleFile(i) = sprintf("../results/rl-perf-tester/vary-work-ct/%d/single.28d.31c.ComputeAndWriteout.dat", BitDepths[i + 1])

plot for [i=0:2] file(i) u (column(1)/1.2e3):(1./5000.) smooth cumul with lines title myTitle(i) ls LineStyles[i + 1] dt DashStyles[i + 1] lw 2#, \
	#for [i=2:2] singleFile(i) u (column(1)/1.2e3):(1./5000.) smooth cumul with lines title singleTitle(i) ls LineStyles[2] dt DashStyles[2]

