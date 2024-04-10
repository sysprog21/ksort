#ifndef KSORT_H
#define KSORT_H

#include <linux/types.h>

typedef int cmp_t(const void *, const void *);

extern struct workqueue_struct *workqueue;

void sort_main(void *sort_buffer, size_t size, size_t es, cmp_t cmp);

#endif
