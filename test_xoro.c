/* Basic test for reading from /dev/xoroshiro128p */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
    int fd = open("/dev/xoroshiro128p", O_RDONLY);
    if (0 > fd) {
        perror("Failed to open the device.");
        return errno;
    }

    /* Test reading different numbers of bytes */
    for (int n_bytes = 0; n_bytes < 10; n_bytes++) {
        /* Clear/zero the buffer before copying in read data */
        zero_rx();

        /* Read the response from the LKM */
        ssize_t n_bytes_read = read(fd, rx, n_bytes);

        if (0 > n_bytes_read) {
            perror("Failed to read all bytes.");
            return errno;
        }

        uint64_t value_ = 0;
        for (int b_idx = 0; b_idx < n_bytes_read; b_idx++) {
            unsigned char b = rx[b_idx];
            value_ |= ((uint64_t) b << (8 * b_idx));
        }
        printf("n_bytes=%d n_bytes_read=%ld value=%016lx\n", n_bytes,
               n_bytes_read, value_);
    }

    return 0;
}
