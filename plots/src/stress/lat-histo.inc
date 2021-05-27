load "gnuplot-palettes/inferno.pal"

set ylabel "Frequency"
set xlabel "Packet Latency (\\si{\\micro\\second})"

set xrange [0:700000]

# based on : http://psy.swansea.ac.uk/staff/carter/gnuplot/gnuplot_frequency.htm

# Each bar s half the (visual) width of its x-range.
set boxwidth 0.05 absolute
#set style fill solid 1.0 noborder

bin_width = 100;

bin_number(x) = floor(x/bin_width)

rounded(x) = bin_width * ( bin_number(x) + 0.5 )

plot_file(k) = sprintf("../results/stress-process/l-%dB-%dk.dat", pktsz, k)
mtitle(k) = sprintf("%dk requests/s", k)

plot plot_file(base_k) using (rounded($1)):(1) smooth frequency with boxes fill pattern 1 ls 3 title mtitle(base_k), \
	plot_file(target_k) using (rounded($1)):(0.75) smooth frequency with boxes ls 6 title mtitle(target_k)

