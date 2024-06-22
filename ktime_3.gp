set terminal pngcairo
set output 'ktime_3.png'
set title "Kernel Sorting Time Comparison(xoro)"
set xlabel "Number of Elements"
set ylabel "Time (ns)"
set grid
set style data linespoints
set yrange [0:5000000]
set key left

plot "ktime_3.dat" using 1:2 title "linuxsort Time" with point, \
     "ktime_3.dat" using 1:3 title "pdqsort Time" with point
