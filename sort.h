#ifndef KSORT_H
#define KSORT_H

#include <linux/types.h>

typedef int cmp_t(const void *, const void *);

typedef void (*swap_func_t)(void *a, void *b, int size);

typedef int (*cmp_r_func_t)(const void *a, const void *b, const void *priv);
typedef int (*cmp_func_t)(const void *a, const void *b);

extern struct workqueue_struct *workqueue;

void sort_main(void *sort_buffer, size_t size, size_t es, cmp_t cmp);

void sort_pdqsort(void *base, size_t num, size_t size, cmp_func_t cmp_func, swap_func_t swap_func);


#endif
