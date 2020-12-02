set xlabel "Q$n$"
set ylabel "Accuracy vs Float32"

getValue(row,col,filename) = system('awk ''{if (NR == '.row.') print $'.col.'}'' '.filename.'')

set arrow from 0.,1.0 to 33.,1.0 nohead front ls 2 dt (18,2,2,2)

plot "../results/marl-quant/process-q-accs.csv" every ::1 u ($0+1):($1/getValue(1,1,"../results/marl-quant/process-q-accs.csv")) with linespoint notitle
