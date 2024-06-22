set terminal pngcairo
set output 'ktime_2__.png'
set title "Kernel Sorting Time Comparison 3(Fisher Yates shuffling)"
set xlabel "Number of Elements"
set ylabel "Time (ns)"
set grid
set style data linespoints
set yrange [0:5000000]
set key left

plot "ktime_2.dat" using 1:2 title "qsort Time" with point, \
     "ktime_2.dat" using 1:3 title "linuxsort Time" with point
