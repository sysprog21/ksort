#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define KSORT_DEV "/dev/sort"

#define MAX_BYTES_PER_READ 8
static unsigned char rx[MAX_BYTES_PER_READ]; /* Receive buffer from the LKM */

void zero_rx(void)
{
    for (int b_idx = 0; b_idx < MAX_BYTES_PER_READ; b_idx++) {
        rx[b_idx] = 0;
    }
}

int main(int argc, char *argv[])
{
    int fd = open(KSORT_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        goto error;
    }

    int fd_xoro = open("/dev/xoro", O_RDONLY);
    if (0 > fd_xoro) {
        perror("Failed to open the device.");
        goto error;
    }



    size_t n_elements = 10000;
    size_t size = n_elements * sizeof(uint64_t);
    uint64_t *inbuf = malloc(size);
    uint64_t *inbuf__ = malloc(size);
    if (!inbuf)
        goto error;

    uint64_t val;
    for (int i = 0; i < n_elements; i++) {
        // Clear/zero the buffer before copying in read data.
        zero_rx();

        // Read the response from the LKM.
        int n_bytes = rand() % 3 + 1;
        ssize_t n_bytes_read = read(fd_xoro, rx, n_bytes);

        if (0 > n_bytes_read) {
            perror("Failed to read all bytes.");
            goto error;
        }

        val = 0;
        for (int b_idx = 0; b_idx < n_bytes_read; b_idx++) {
            unsigned char b = rx[b_idx];
            val |= ((uint64_t) b << (8 * b_idx));
        }
        inbuf__[i] = val;
        // printf("n_bytes=%d n_bytes_read=%ld value=%016lx\n", n_bytes,
        //        n_bytes_read, value_);
    }

    for (size_t i = 0; i < n_elements; i++)
        inbuf[i] = (int) inbuf__[i];  // n_elements - i;

    for (size_t i = 0; i < n_elements; i++)
        printf("before inbuf[%lu] :  %ld\n", i, inbuf[i]);

    int *set_sort = malloc(sizeof(int));
    *set_sort = atoi(argv[1]);
    if (*set_sort == 3) {
        printf("sort by pdqsort\n");
    } else if (*set_sort == 2) {
        printf("sort by linux lib sort\n");
    } else {
        printf("sort by qsort\n");
    }
    ssize_t set_res = write(fd, set_sort, sizeof(int));
    if (set_res > 0) {
        printf("set seccuess  %zd\n", set_res);
    } else {
        printf("set failed\n");
    }
    free(set_sort);



    ssize_t r_time = read(fd, inbuf, size);
    if (r_time <= 0) {
        perror("Failed to write character device");
        goto error;
    }


    bool pass = true;
    int ret = 0;
    // Verify the result of sorting
    for (size_t i = 1; i < n_elements; i++) {
        printf("after inbuf[%ld] : %ld\n", i - 1, inbuf[i - 1]);
        if (inbuf[i] < inbuf[i - 1]) {
            pass = false;
            break;
        }
    }
    printf("after inbuf[%d] : %ld\n", 999, inbuf[999]);

    printf("Soring %s!\n", pass ? "succeeded" : "failed");

error:
    free(inbuf);
    if (fd > 0)
        close(fd);
    return ret;
}
