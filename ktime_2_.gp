set terminal pngcairo
set output 'ktime_2_.png'
set title "Kernel Sorting Time Comparison 2(Fisher Yates shuffling)"
set xlabel "Number of Elements"
set ylabel "Time (ns)"
set grid
set style data linespoints
set yrange [0:5000000]
set key left

plot "ktime_2.dat" using 1:2 title "qsort Time" with point, \
     "ktime_2.dat" using 1:4 title "pdqsort Time" with point
