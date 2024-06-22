set terminal pngcairo
set output 'ktime_1.png'
set title "Kernel Sorting Time Comparison(descending order)"
set xlabel "Number of Elements"
set ylabel "Time (ns)"
set grid
set style data linespoints
set yrange [0:5000000]
set key left

plot "ktime_1.dat" using 1:2 title "qsort Time" with point, \
     "ktime_1.dat" using 1:3 title "linuxsort Time" with point, \
     "ktime_1.dat" using 1:4 title "pdqsort Time" with point
