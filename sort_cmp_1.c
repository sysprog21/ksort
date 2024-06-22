#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define KSORT_DEV "/dev/sort"

int main()
{
    int fd = open(KSORT_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        goto error;
    }

    FILE *f_qsort = fopen("qsort_time_1.dat", "w");
    if (f_qsort == NULL) {
        printf("unable to open file\n");
        return 0;
    }
    FILE *f_linuxsort = fopen("linuxsort_time_1.dat", "w");
    if (f_linuxsort == NULL) {
        printf("unable to open file\n");
        return 0;
    }
    FILE *f_pdqsort = fopen("pdqsort_time_1.dat", "w");
    if (f_pdqsort == NULL) {
        printf("unable to open file\n");
        return 0;
    }

    FILE *f_ktime = fopen("ktime_1.dat", "w");
    if (f_ktime == NULL) {
        printf("unable to open file\n");
        return 0;
    }

    struct timespec start, end;

    int sort_time[10000][4];


    int *set_sort = malloc(sizeof(int));
    if (!set_sort) {
        printf("set_sort malloc failed");
        goto error;
    }

    size_t set_res, k;

    for (size_t i = 1; i <= 10000; i++) {
        size_t n_elements = i;
        size_t size = n_elements * sizeof(uint64_t);
        uint64_t *inbuf_qsort = malloc(size);
        uint64_t *inbuf_linuxsort = malloc(size);
        uint64_t *inbuf_pdqsort = malloc(size);
        if (!inbuf_qsort)
            goto error;
        if (!inbuf_linuxsort)
            goto error;
        if (!inbuf_pdqsort)
            goto error;

        for (size_t j = 0; j < n_elements; j++) {
            inbuf_qsort[j] = n_elements - j;
            inbuf_linuxsort[j] = n_elements - j;
            inbuf_pdqsort[j] = n_elements - j;
        }

        sort_time[i - 1][0] = i;

        // qsort measure
        *set_sort = 1;
        set_res = write(fd, set_sort, sizeof(int));
        if (set_res <= 0) {
            printf("set failed\n");
            free(set_sort);
            goto error;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        int r_time = read(fd, inbuf_qsort, size);
        clock_gettime(CLOCK_MONOTONIC, &end);

        int time_spend_qsort = (end.tv_sec - start.tv_sec) * 1000000000LL +
                               (end.tv_nsec - start.tv_nsec);
        printf("qsort time :  %d\n", time_spend_qsort);

        sort_time[i - 1][1] = r_time;


        bool pass = true;

        for (k = 1; k < n_elements; k++) {
            if (inbuf_qsort[k] < inbuf_qsort[k - 1]) {
                pass = false;
                break;
            }
        }

        fprintf(f_qsort, "%lu %d\n", i, time_spend_qsort);
        fprintf(f_ktime, "%ld %d   ", i, r_time);

        free(inbuf_qsort);

        printf("qsort %s  %ld!\n", pass ? "succeeded" : "failed", i);

        // linuxsort measure
        *set_sort = 2;
        set_res = write(fd, set_sort, sizeof(int));
        if (set_res <= 0) {
            printf("set failed\n");
            free(set_sort);
            goto error;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        r_time = read(fd, inbuf_linuxsort, size);
        clock_gettime(CLOCK_MONOTONIC, &end);

        int time_spend_linuxsort = (end.tv_sec - start.tv_sec) * 1000000000LL +
                                   (end.tv_nsec - start.tv_nsec);
        printf("linuxsort time :  %d\n", time_spend_linuxsort);

        sort_time[i - 1][2] = r_time;


        pass = true;

        for (k = 1; k < n_elements; k++) {
            if (inbuf_linuxsort[k] < inbuf_linuxsort[k - 1]) {
                pass = false;
                break;
            }
        }

        fprintf(f_linuxsort, "%ld %d\n", i, time_spend_linuxsort);
        fprintf(f_ktime, "%d   ", r_time);

        free(inbuf_linuxsort);

        printf("linuxsort %s  %ld!\n", pass ? "succeeded" : "failed", i);

        // pdqsort measure
        *set_sort = 3;
        set_res = write(fd, set_sort, sizeof(int));
        if (set_res <= 0) {
            printf("set failed\n");
            free(set_sort);
            goto error;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        r_time = read(fd, inbuf_pdqsort, size);
        clock_gettime(CLOCK_MONOTONIC, &end);

        int time_spend_pdqsort = (end.tv_sec - start.tv_sec) * 1000000000LL +
                                 (end.tv_nsec - start.tv_nsec);
        printf("pdqsort time :  %d\n", time_spend_pdqsort);

        sort_time[i - 1][3] = r_time;


        pass = true;

        for (k = 1; k < n_elements; k++) {
            if (inbuf_pdqsort[k] < inbuf_pdqsort[k - 1]) {
                pass = false;
                break;
            }
        }

        fprintf(f_pdqsort, "%ld %d\n", i, time_spend_pdqsort);
        fprintf(f_ktime, "%d\n", r_time);

        free(inbuf_pdqsort);

        printf("pdqsort %s  %ld!\n", pass ? "succeeded" : "failed", i);
    }

    for (int i = 0; i < 10000; i++) {
        for (int j = 0; j < 4; j++) {
            printf("%d    ", sort_time[i][j]);
        }
        printf("\n");
    }


    free(set_sort);

error:
    fclose(f_qsort);
    fclose(f_linuxsort);
    fclose(f_pdqsort);
    fclose(f_ktime);
    if (fd > 0)
        close(fd);
    return 0;
}
