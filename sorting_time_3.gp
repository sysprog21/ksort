set terminal pngcairo
set output 'sorting_time_3.png'
set title "Sorting Time Comparison(xoro)"
set xlabel "Number of Elements"
set ylabel "Time (ns)"
set grid
set style data linespoints
set yrange [0:5000000]
set key left

plot "linuxsort_time_3.dat" using 1:2 title "linuxsort Time" with point, \
     "pdqsort_time_3.dat" using 1:2 title "pdqsort Time" with point
