#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define KSORT_DEV "/dev/sort"

int main()
{
    int ret = EXIT_SUCCESS;
    int fd = open(KSORT_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        ret = EXIT_FAILURE;
        goto error_open;
    }

    size_t n_elements = 1000;
    size_t size = n_elements * sizeof(int);
    int *inbuf = malloc(size);
    if (!inbuf) {
        perror("Failed to allocate memory");
        ret = EXIT_FAILURE;
        goto error_alloc;
    }

    for (size_t i = 0; i < n_elements; i++)
        inbuf[i] = rand() % n_elements;

    ssize_t r_sz = read(fd, inbuf, size);
    if (r_sz != size) {
        perror("Failed to read character device");
        ret = EXIT_FAILURE;
        goto error_read;
    }

    bool pass = true;
    /* Verify the result of sorting */
    for (size_t i = 1; i < n_elements; i++) {
        if (inbuf[i] < inbuf[i - 1]) {
            pass = false;
            break;
        }
    }

    printf("Sorting %s!\n", pass ? "succeeded" : "failed");

error_read:
    free(inbuf);
error_alloc:
    close(fd);
error_open:
    return ret;
}
