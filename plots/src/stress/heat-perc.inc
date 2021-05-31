set terminal tikz standalone color size 13cm,9cm font '\Large' preamble '\usepackage{microtype} \usepackage{libertinus} \usepackage{libertinust1math} \usepackage[binary-units, per-mode=symbol]{siunitx}\sisetup{detect-all} \newcommand{\approachshort}{OPaL} \newcommand{\Coopfw}{CoOp} \newcommand{\coopfw}{\Coopfw} \newcommand{\Indfw}{Ind} \newcommand{\indfw}{\Indfw}'

load "gnuplot-palettes/gnbu.pal"

set xlabel "Packet Size (\\si{\\byte})"
set ylabel "RL Request rate (k updates/s)" offset -2
set cblabel "Packet Latency Deviation (\\si{\\percent})" offset 4
set view map

set xrange [-0.5:6.5]
set yrange [-0.5:16.5]

array PktSzs[7]
PktSzs[1]=64
PktSzs[2]=128
PktSzs[3]=256
PktSzs[4]=512
PktSzs[5]=1024
PktSzs[6]=1280
PktSzs[7]=1518

unset xtics
set tics textcolor rgb "black"
do for [i=1:7] {set xtics add (sprintf("%d", PktSzs[i]) i-1)}

set datafile separator " "

threshold = 0.05

plot sprintf('../results/stress-process/matrix-latency-%sperc.dat', metric) matrix w image pixel notitle, \
	'../results/stress-process/matrix-latency-p.dat' matrix u 1:2:($3 >= threshold ? "" : "+") with labels tc 'black' notitle

