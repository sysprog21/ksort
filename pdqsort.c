/*
    pdqsort.h - Pattern-defeating quicksort.
    Copyright (c) 2021 Orson Peters
    This software is provided 'as-is', without any express or implied warranty.
   In no event will the authors be held liable for any damages arising from the
   use of this software. Permission is granted to anyone to use this software
   for any purpose, including commercial applications, and to alter it and
   redistribute it freely, subject to the following restrictions:
    1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software in a
   product, an acknowledgment in the product documentation would be appreciated
   but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
*/

#include <linux/compiler.h>
#include <linux/limits.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "sort.h"

#define insertion_sort_threshold 24
#define ninther_threshold 128
#define partial_insertion_sort_limit 8

#define idx(x) (x) * size

typedef void (*swap_func_t)(void *a, void *b, int size);
typedef int (*cmp_r_func_t)(const void *a, const void *b, const void *priv);
typedef int (*cmp_func_t)(const void *a, const void *b);

static inline int __log2(size_t x)
{
    return 63 - __builtin_clzll(x);
}

static int cmpint64(const void *a, const void *b)
{
    uint64_t a_val = *(uint64_t *) a;
    uint64_t b_val = *(uint64_t *) b;
    if (a_val > b_val)
        return 1;
    if (a_val == b_val)
        return 0;
    return -1;
}

/**
 * swap_words_32 - swap two elements in 32-bit chunks
 * @a: pointer to the first element to swap
 * @b: pointer to the second element to swap
 * @n: element size (must be a multiple of 4)
 *
 * Exchange the two objects in memory.  This exploits base+index addressing,
 * which basically all CPUs have, to minimize loop overhead computations.
 *
 * For some reason, on x86 gcc 7.3.0 adds a redundant test of n at the
 * bottom of the loop, even though the zero flag is stil valid from the
 * subtract (since the intervening mov instructions don't alter the flags).
 * Gcc 8.1.0 doesn't have that problem.
 */
static void swap_words_32(void *_a, void *_b, size_t n)
{
    char *a = _a, *b = _b;
    do {
        uint32_t t = *(uint32_t *) (a + (n -= 4));
        *(uint32_t *) (a + n) = *(uint32_t *) (b + n);
        *(uint32_t *) (b + n) = t;
    } while (n);
}

/**
 * swap_words_64 - swap two elements in 64-bit chunks
 * @a: pointer to the first element to swap
 * @b: pointer to the second element to swap
 * @n: element size (must be a multiple of 8)
 *
 * Exchange the two objects in memory.  This exploits base+index
 * addressing, which basically all CPUs have, to minimize loop overhead
 * computations.
 *
 * We'd like to use 64-bit loads if possible.  If they're not, emulating
 * one requires base+index+4 addressing which x86 has but most other
 * processors do not.  If CONFIG_64BIT, we definitely have 64-bit loads,
 * but it's possible to have 64-bit loads without 64-bit pointers (e.g.
 * x32 ABI).  Are there any cases the kernel needs to worry about?
 */
static void swap_words_64(void *_a, void *_b, size_t n)
{
    char *a = _a, *b = _b;
    do {
#ifdef CONFIG_64BIT
        uint64_t t = *(uint64_t *) (a + (n -= 8));
        *(uint64_t *) (a + n) = *(uint64_t *) (b + n);
        *(uint64_t *) (b + n) = t;
#else
        /* Use two 32-bit transfers to avoid base+index+4 addressing */
        uint32_t t = *(uint32_t *) (a + (n -= 4));
        *(uint32_t *) (a + n) = *(uint32_t *) (b + n);
        *(uint32_t *) (b + n) = t;

        t = *(uint32_t *) (a + (n -= 4));
        *(uint32_t *) (a + n) = *(uint32_t *) (b + n);
        *(uint32_t *) (b + n) = t;
#endif
    } while (n);
}

/**
 * swap_bytes - swap two elements a byte at a time
 * @a: pointer to the first element to swap
 * @b: pointer to the second element to swap
 * @n: element size
 *
 * This is the fallback if alignment doesn't allow using larger chunks.
 */
static void swap_bytes(void *a, void *b, size_t n)
{
    do {
        char t = ((char *) a)[--n];
        ((char *) a)[n] = ((char *) b)[n];
        ((char *) b)[n] = t;
    } while (n);
}

/*
 * The values are arbitrary as long as they can't be confused with
 * a pointer, but small integers make for the smallest compare
 * instructions.
 */
#define SWAP_WORDS_64 (swap_func_t) 0
#define SWAP_WORDS_32 (swap_func_t) 1
#define SWAP_BYTES (swap_func_t) 2

#if 0
#define BLOCK_SIZE 64
#define CACHELINE_SIZE 64
#endif

/*
 * The function pointer is last to make tail calls most efficient if the
 * compiler decides not to inline this function.
 */
static void do_swap(void *a, void *b, size_t size, swap_func_t swap_func)
{
    if (swap_func == SWAP_WORDS_64)
        swap_words_64(a, b, size);
    else if (swap_func == SWAP_WORDS_32)
        swap_words_32(a, b, size);
    else if (swap_func == SWAP_BYTES)
        swap_bytes(a, b, size);
    else
        swap_func(a, b, (int) size);
}

#if 0
static void print(void *_begin, void *_end, size_t size)
{
    
    for (int i = 0, char *cur = (char *)_begin; cur != (char *)_end; cur += idx(1), i++) {
        printf("%d %lu\n", i, *(uint64_t *) cur);
    }
    printf("\n");
}
#endif

void insertion_sort(void *_begin, void *_end, size_t size, cmp_func_t cmp_func)
{
    char *begin = (char *) _begin;
    char *end = (char *) _end;

    if (begin == end)
        return;

    for (char *cur = begin + size; cur != end; cur += size) {
        char *sift = cur;
        char *sift_1 = cur - size;

        if (cmp_func(sift, sift_1)) {
            do {
                do_swap(sift, sift_1, size, 0);
                sift -= size;
            } while (sift != begin && cmp_func(sift, sift_1 -= size));
        }
    }
}

void unguarded_insertion_sort(void *_begin,
                              void *_end,
                              size_t size,
                              cmp_func_t cmp_func)
{
    char *begin = (char *) _begin;
    char *end = (char *) _end;

    if (begin == end)
        return;

    for (char *cur = begin + size; cur != end; cur += size) {
        char *sift = cur;
        char *sift_1 = cur - size;

        if (cmp_func(sift, sift_1)) {
            do {
                do_swap(sift, sift_1, size, 0);
                sift -= size;
            } while (cmp_func(sift, sift_1 -= size));
        }
    }
}

static bool partial_insertion_sort(void *_begin,
                                   void *_end,
                                   size_t size,
                                   cmp_func_t cmp_func)
{
    char *begin = (char *) _begin;
    char *end = (char *) _end;

    if (begin == end)
        return true;

    size_t limit = 0;
    for (char *cur = begin + size; cur != end; cur += size) {
        char *sift = cur;
        char *sift_1 = cur - size;

        if (cmp_func(sift, sift_1)) {
            do {
                do_swap(sift, sift_1, size, 0);
                sift -= size;
            } while (sift != begin && cmp_func(sift, sift_1 -= size));

            limit += (cur - sift) / size;
        }

        if (limit > partial_insertion_sort_limit)
            return false;
    }

    return true;
}

static void sort2(void *a, void *b, size_t size, cmp_func_t cmp_func)
{
    if (cmp_func(b, a))
        do_swap(a, b, size, 0);
}

static void sort3(void *a, void *b, void *c, size_t size, cmp_func_t cmp_func)
{
    sort2(a, b, size, cmp_func);
    sort2(b, c, size, cmp_func);
    sort2(a, b, size, cmp_func);
}

#if 0
static void swap_offsets(char *first,
                         char *last,
                         size_t size,
                         char *offsets_l,
                         char *offsets_r,
                         size_t num,
                         bool use_swaps)
{
    if (use_swaps) {
        // This case is needed for the descending distribution, where we need
        // to have proper swapping for pdqsort to remain O(n).
        for (size_t i = 0; i < num; ++i) {
            do_swap(first + idx(offsets_l[i]), last - idx(offsets_r[i]), size,
                    0);
        }
    } else if (num > 0) {
        for (size_t i = 0; i < num; ++i) {
            char *l, *r;
            l = first + idx(offsets_l[i]);
            r = last - idx(offsets_r[i]);
            do_swap(l, r, size, 0);
        }
    }
}

static uintptr_t align_cacheline(char *p)
{
    uintptr_t lp = (uintptr_t) p;
    lp = (lp + CACHELINE_SIZE - 1) & -CACHELINE_SIZE;
    return lp;
}

static bool partition_right_branchless(void *_begin,
                                       void *_end,
                                       size_t size,
                                       cmp_func_t cmp_func,
                                       char **ret_pivot)
{
    char *begin = (char *) _begin;
    char *end = (char *) _end;

    char *first = begin;
    char *last = end;

    while (cmp_func(first += idx(1), &begin))
        ;

    if (first - idx(1) == begin)
        while (first < last && !cmp_func(last -= idx(1), &begin))
            ;
    else
        while (!cmp_func(last -= idx(1), &begin))
            ;

    bool already_partitioned = first >= last;
    if (!already_partitioned) {
        do_swap(first, last, size, 0);
        first += idx(1);

        char offsets_l_storage[BLOCK_SIZE + CACHELINE_SIZE];
        char offsets_r_storage[BLOCK_SIZE + CACHELINE_SIZE];

        // char offsets_l_storage[BLOCK_SIZE + CACHELINE_SIZE];
        // char offsets_r_storage[BLOCK_SIZE + CACHELINE_SIZE];

        // char *offsets_l = offsets_l_storage;
        // char *offsets_r = offsets_r_storage;

        char *offsets_l = (char *) align_cacheline(offsets_l_storage);
        char *offsets_r = (char *) align_cacheline(offsets_r_storage);

        char *offsets_l_base = first;
        char *offsets_r_base = last;
        size_t num_l, num_r, start_l, start_r;
        num_l = num_r = start_l = start_r = 0;
        while (first < last) {
            size_t num_unknown = (last - first) / size;
            size_t left_split =
                num_l == 0 ? (num_r == 0 ? num_unknown / 2 : num_unknown) : 0;
            size_t right_split = num_r == 0 ? (num_unknown - left_split) : 0;

            if (left_split >= BLOCK_SIZE) {
                for (size_t i = 0; i < BLOCK_SIZE;) {
                    offsets_l[num_l] = i++;
                    num_l += !cmp_func(first, begin);
                    first += size;
                    offsets_l[num_l] = i++;
                    num_l += !cmp_func(first, begin);
                    first += size;
                    offsets_l[num_l] = i++;
                    num_l += !cmp_func(first, begin);
                    first += size;
                    offsets_l[num_l] = i++;
                    num_l += !cmp_func(first, begin);
                    first += size;
                    offsets_l[num_l] = i++;
                    num_l += !cmp_func(first, begin);
                    first += size;
                    offsets_l[num_l] = i++;
                    num_l += !cmp_func(first, begin);
                    first += size;
                    offsets_l[num_l] = i++;
                    num_l += !cmp_func(first, begin);
                    first += size;
                    offsets_l[num_l] = i++;
                    num_l += !cmp_func(first, begin);
                    first += size;
                }
            } else {
                for (size_t i = 0; i < left_split;) {
                    offsets_l[num_l] = i++;
                    num_l += !cmp_func(first, begin);
                    first += size;
                }
            }

            if (right_split >= BLOCK_SIZE) {
                for (size_t i = 0; i < BLOCK_SIZE;) {
                    offsets_r[num_r] = ++i;
                    last -= size;
                    num_r += cmp_func(last, begin);
                    offsets_r[num_r] = ++i;
                    last -= size;
                    num_r += cmp_func(last, begin);
                    offsets_r[num_r] = ++i;
                    last -= size;
                    num_r += cmp_func(last, begin);
                    offsets_r[num_r] = ++i;
                    last -= size;
                    num_r += cmp_func(last, begin);
                    offsets_r[num_r] = ++i;
                    last -= size;
                    num_r += cmp_func(last, begin);
                    offsets_r[num_r] = ++i;
                    last -= size;
                    num_r += cmp_func(last, begin);
                    offsets_r[num_r] = ++i;
                    last -= size;
                    num_r += cmp_func(last, begin);
                    offsets_r[num_r] = ++i;
                    last -= size;
                    num_r += cmp_func(last, begin);
                }
            } else {
                for (size_t i = 0; i < right_split;) {
                    offsets_r[num_r] = ++i;
                    last -= size;
                    num_r += cmp_func(last, begin);
                }
            }

            size_t num = num_l < num_r ? num_l : num_r;
            swap_offsets(offsets_l_base, offsets_r_base, size,
                         offsets_l + start_l, offsets_r + start_r, num,
                         num_l == num_r);
            num_l -= num;
            num_r -= num;
            start_l += num;
            start_r += num;
            if (!num_l) {
                start_l = 0;
                offsets_l_base = first;
            }

            if (!num_r) {
                start_r = 0;
                offsets_r_base = last;
            }
        }
        // We have now fully identified [first, last)'s proper position. Swap
        // the last elements.
        if (num_l) {
            offsets_l += start_l;
            while (num_l--)
                do_swap(offsets_l_base + idx(offsets_l[num_l]), (last -= size),
                        size, 0);
            first = last;
        }
        if (num_r) {
            offsets_r += start_r;
            while (num_r--) {
                do_swap(offsets_r_base - idx(offsets_r[num_r]), first, size, 0);
                first += size;
            }
            last = first;
        }
    }
    do_swap(begin, first - size, size, 0);

    *ret_pivot = first - size;

    return first - size;
}
#endif

static bool partition_right(void *_begin,
                            void *_end,
                            size_t size,
                            cmp_func_t cmp_func,
                            char **ret_pivot)
{
    char *begin = (char *) _begin;
    char *end = (char *) _end;

    char *first = begin;
    char *last = end;

    while (cmp_func(first += size, begin))
        ;

    if (first - size == begin)
        while (first < last && !cmp_func(last -= size, begin))
            ;
    else
        while (!cmp_func(last -= size, begin))
            ;

    bool already_partitioned = first >= last;

    while (first < last) {
        do_swap(first, last, size, 0);
        while (cmp_func(first += size, begin))
            ;
        while (!cmp_func(last -= size, begin))
            ;
    }

    do_swap(begin, first - size, size, 0);

    *ret_pivot = first - size;

    return already_partitioned;
}

static char *partition_left(void *_begin,
                            void *_end,
                            size_t size,
                            cmp_func_t cmp_func)
{
    char *begin = (char *) _begin;
    char *end = (char *) _end;

    char *first = begin;
    char *last = (char *) _end;

    while (cmp_func(begin, (last -= size)))
        ;

    if (last + size == end)
        while (first < last && !cmp_func(begin, (first += size)))
            ;
    else
        while (!cmp_func(begin, (first += size)))
            ;

    while (first < last) {
        do_swap(first, last, size, 0);
        while (cmp_func(begin, (last -= size)))
            ;
        while (!cmp_func(begin, (first += size)))
            ;
    }

    do_swap(begin, last, size, 0);

    return last;
}

static void pdqsort_loop(void *_begin,
                         void *_end,
                         size_t size,
                         cmp_func_t cmp_func,
                         size_t max_depth,
                         bool leftmost)
{
    char *begin = (char *) _begin;
    char *end = (char *) _end;
    while (true) {
        size_t num = (end - begin) / size;

        if (num < insertion_sort_threshold) {
            if (leftmost)
                insertion_sort(begin, end, size, cmp_func);
            else
                unguarded_insertion_sort(begin, end, size, cmp_func);
            return;
        }
        size_t m = num / 2;
        if (num > ninther_threshold) {
            sort3(begin, begin + idx(m), end - idx(1), size, cmp_func);
            sort3(begin + idx(1), begin + idx(m - 1), end - idx(2), size,
                  cmp_func);
            sort3(begin + idx(2), begin + idx(m + 1), end - idx(3), size,
                  cmp_func);
            sort3(begin + idx(m - 1), begin + idx(m), begin + idx(m + 1), size,
                  cmp_func);
            do_swap(begin, begin + idx(m), size, 0);
        } else {
            sort3(begin, begin + idx(m), end - idx(1), size, cmp_func);
        }
        if (!leftmost && !cmp_func(begin - idx(1), begin)) {
            begin = partition_left(begin, end, size, cmp_func) + idx(1);
            continue;
        }
        char *pivot;
        bool already_partitioned =
            partition_right(begin, end, size, cmp_func, &pivot);

        size_t l_size = (pivot - begin) / size;
        size_t r_size = (end - (pivot + idx(1))) / size;
        bool highly_unbalanced = l_size < num / 8 || r_size < num / 8;

        if (likely(highly_unbalanced)) {
            if (--max_depth == 0) {
                size_t part_length = (size_t) ((end - begin) / size);
                if (part_length > 0) {
                    size_t i, j, k = part_length >> 1;
                    char *tmp = kmalloc(size, GFP_KERNEL);
                    /* heapification */
                    do {
                        i = k;
                        j = (i << 1) + 2;
                        memcpy(tmp, begin + idx(i), size);

                        while (j <= part_length) {
                            if (j < part_length)
                                j += (cmpint64(begin + idx(j),
                                               begin + idx(j + 1)) < 0);
                            if (cmpint64(begin + idx(j), tmp) <= 0)
                                break;
                            memcpy(begin + idx(i), begin + idx(j), size);
                            i = j;
                            j = (i << 1) + 2;
                        }

                        memcpy(begin + idx(i), tmp, size);
                    } while (k-- > 0);

                    /* heapsort */
                    do {
                        i = part_length;
                        j = 0;
                        memcpy(tmp, begin + idx(part_length), size);

                        /* Floyd's optimization:
                         * Not checking low[j] <= tmp saves nlog2(n) comparisons
                         */
                        while (j < part_length) {
                            if (j < part_length - 1)
                                j += (cmpint64(begin + idx(j),
                                               begin + idx(j + 1)) < 0);
                            memcpy(begin + idx(i), begin + idx(j), size);
                            i = j;
                            j = (i << 1) + 2;
                        }

                        /* Compensate for Floyd's optimization by sifting down
                         * tmp. This adds O(n) comparisons and moves.
                         */
                        while (i > 1) {
                            j = (i - 2) >> 1;
                            if (cmpint64(tmp, begin + idx(j)) <= 0)
                                break;
                            memcpy(begin + idx(i), begin + idx(j), size);
                            i = j;
                        }

                        memcpy(begin + idx(i), tmp, size);
                    } while (part_length-- > 0);
                    kfree(tmp);
                }
                return;
            }
            if (l_size >= insertion_sort_threshold) {
                do_swap(begin, begin + idx(l_size / 4), size, 0);
                do_swap(pivot - idx(1), pivot - idx(l_size / 4), size, 0);

                if (l_size > ninther_threshold) {
                    do_swap(begin + idx(1), begin + idx(l_size / 4 + 1), size,
                            0);
                    do_swap(begin + idx(2), begin + idx(l_size / 4 + 2), size,
                            0);
                    do_swap(pivot - idx(2), pivot - idx(l_size / 4 + 1), size,
                            0);
                    do_swap(pivot - idx(3), pivot - idx(l_size / 4 + 2), size,
                            0);
                }
            }

            if (r_size >= insertion_sort_threshold) {
                do_swap(pivot + idx(1), pivot + idx((1 + r_size / 4)), size, 0);
                do_swap(end - idx(1), end - idx(r_size / 4), size, 0);

                if (r_size > ninther_threshold) {
                    do_swap(pivot + idx(2), pivot + idx(2 + r_size / 4), size,
                            0);
                    do_swap(pivot + idx(3), pivot + idx(3 + r_size / 4), size,
                            0);
                    do_swap(end - idx(2), end - idx(1 + r_size / 4), size, 0);
                    do_swap(end - idx(3), end - idx(2 + r_size / 4), size, 0);
                }
            }
        } else {
            if (already_partitioned &&
                partial_insertion_sort(begin, pivot, size, cmp_func) &&
                partial_insertion_sort(pivot + idx(1), end, size, cmp_func)) {
                return;
            }
        }

        pdqsort_loop(begin, pivot, size, cmp_func, max_depth, leftmost);
        begin = pivot + idx(1);
        leftmost = false;
    }
}

void sort_pdqsort(void *base,
                  size_t num,
                  size_t size,
                  cmp_func_t cmp_func,
                  swap_func_t swap_func)
{
    pdqsort_loop(base, (char *) base + idx(num), size, cmp_func, __log2(num),
                 true);
}
