set terminal pngcairo
set output 'sorting_time_2__.png'
set title "Sorting Time Comparison 3(Fisher Yates shuffling)"
set xlabel "Number of Elements"
set ylabel "Time (ns)"
set grid
set style data linespoints
set yrange [0:5000000]
set key left

plot "qsort_time_2.dat" using 1:2 title "qsort Time" with point, \
     "linuxsort_time_2.dat" using 1:2 title "linuxsort Time" with point
