load "gnuplot-palettes/gnbu.pal"

set xlabel "Packet Size (\\si{\\byte})"
set ylabel "RL Request rate (k updates/s)"
set cblabel "Median Packet Latency (\\si{\\micro\\second})"
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

plot '../results/stress-process/matrix-latency-median.dat' matrix w image pixel notitle

