set terminal tikz standalone color size 6cm,4cm preamble '\usepackage{microtype} \usepackage{libertinus} \usepackage{libertinust1math} \usepackage[binary-units, per-mode=symbol]{siunitx}\sisetup{detect-all} \newcommand{\approachshort}{OPaL} \newcommand{\Coopfw}{CoOp} \newcommand{\coopfw}{\Coopfw} \newcommand{\Indfw}{Ind} \newcommand{\indfw}{\Indfw}'

load "gnuplot-palettes/inferno.pal"

set ylabel "Frequency"
set xlabel "Packet Latency (\\si{\\micro\\second})"

#set xtics offset 2
set key above

# set xrange [0:700000]

# based on : http://psy.swansea.ac.uk/staff/carter/gnuplot/gnuplot_frequency.htm

# Each bar s half the (visual) width of its x-range.
#set boxwidth 0.05 absoluteset boxwidth 0.8 absoluteset boxwidth0.8 absolute 
#set style fill solid 1.0 noborder

#set logscale y

bin_width = 200.0 / 1000.0;

# set boxwidth (0.8 * bin_width) absolute

plot_file(k) = sprintf("../results/stress-process/hl-%dB-%dk.dat", pktsz, k)
mtitle(k) = sprintf("%dk/s", k)

set style line 3 lw 0.1
set style line 6 lw 0.1

plot plot_file(base_k) using 1:2:(bin_width) with boxes fill solid ls 3 title mtitle(base_k), \
	plot_file(target_k) using 1:2:(bin_width / 3.0) with boxes fill pattern 1 ls 6 title mtitle(target_k)
