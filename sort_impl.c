#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>

#include "sort.h"

static inline char *med3(char *, char *, char *, cmp_t *, void *);
static inline void swapfunc(char *, char *, int, int);

/* Qsort routine from Bentley & McIlroy's "Engineering a Sort Function" */
#define swapcode(TYPE, parmi, parmj, n) \
    {                                   \
        long i = (n) / sizeof(TYPE);    \
        TYPE *pi = (TYPE *) (parmi);    \
        TYPE *pj = (TYPE *) (parmj);    \
        do {                            \
            TYPE t = *pi;               \
            *pi++ = *pj;                \
            *pj++ = t;                  \
        } while (--i > 0);              \
    }

static inline void swapfunc(char *a, char *b, int n, int swaptype)
{
    if (swaptype <= 1)
        swapcode(long, a, b, n) else swapcode(char, a, b, n)
}

#define q_swap(a, b)                       \
    do {                                   \
        if (swaptype == 0) {               \
            long t = *(long *) (a);        \
            *(long *) (a) = *(long *) (b); \
            *(long *) (b) = t;             \
        } else                             \
            swapfunc(a, b, es, swaptype);  \
    } while (0)

#define vecswap(a, b, n)                 \
    do {                                 \
        if ((n) > 0)                     \
            swapfunc(a, b, n, swaptype); \
    } while (0)

#define CMP(t, x, y) (cmp((x), (y)))

static inline char *med3(char *a,
                         char *b,
                         char *c,
                         cmp_t *cmp,
                         __attribute__((unused)) void *thunk)
{
    return CMP(thunk, a, b) < 0
               ? (CMP(thunk, b, c) < 0 ? b : (CMP(thunk, a, c) < 0 ? c : a))
               : (CMP(thunk, b, c) > 0 ? b : (CMP(thunk, a, c) < 0 ? a : c));
}
struct common {
    int swaptype; /* Code to use for swapping */
    size_t es;    /* Element size. */
    cmp_t *cmp;   /* Comparison function */
};

struct qsort {
    struct work_struct w;
    struct common *common;
    void *a;
    size_t n;
};

#define thunk NULL
static void qsort_algo(struct work_struct *w);

static void init_qsort(struct qsort *q,
                       void *elems,
                       size_t size,
                       struct common *common)
{
    INIT_WORK(&q->w, qsort_algo);
    q->a = elems;
    q->n = size;
    q->common = common;
}

static void qsort_algo(struct work_struct *w)
{
    struct qsort *qs = container_of(w, struct qsort, w);

    bool do_free = true;
    char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
    int d, r, swaptype, swap_cnt;
    void *a;      /* Array of elements. */
    size_t n, es; /* Number of elements; size. */
    cmp_t *cmp;
    size_t nl, nr;
    struct common *c;

    /* Initialize qsort arguments. */
    c = qs->common;
    es = c->es;
    cmp = c->cmp;
    swaptype = c->swaptype;
    a = qs->a;
    n = qs->n;
top:
    /* From here on qsort(3) business as usual. */
    swap_cnt = 0;
    if (n < 7) {
        for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
            for (pl = pm; pl > (char *) a && CMP(thunk, pl - es, pl) > 0;
                 pl -= es)
                q_swap(pl, pl - es);
        return;
    }
    pm = (char *) a + (n / 2) * es;
    if (n > 7) {
        pl = (char *) a;
        pn = (char *) a + (n - 1) * es;
        if (n > 40) {
            d = (n / 8) * es;
            pl = med3(pl, pl + d, pl + 2 * d, cmp, thunk);
            pm = med3(pm - d, pm, pm + d, cmp, thunk);
            pn = med3(pn - 2 * d, pn - d, pn, cmp, thunk);
        }
        pm = med3(pl, pm, pn, cmp, thunk);
    }
    q_swap(a, pm);
    pa = pb = (char *) a + es;

    pc = pd = (char *) a + (n - 1) * es;
    for (;;) {
        while (pb <= pc && (r = CMP(thunk, pb, a)) <= 0) {
            if (r == 0) {
                swap_cnt = 1;
                q_swap(pa, pb);
                pa += es;
            }
            pb += es;
        }
        while (pb <= pc && (r = CMP(thunk, pc, a)) >= 0) {
            if (r == 0) {
                swap_cnt = 1;
                q_swap(pc, pd);
                pd -= es;
            }
            pc -= es;
        }
        if (pb > pc)
            break;
        q_swap(pb, pc);
        swap_cnt = 1;
        pb += es;
        pc -= es;
    }

    pn = (char *) a + n * es;
    r = min(pa - (char *) a, pb - pa);
    vecswap(a, pb - r, r);
    r = min(pd - pc, pn - pd - (long) es);
    vecswap(pb, pn - r, r);

    if (swap_cnt == 0) { /* Switch to insertion sort */
        r = 1 + n / 4;   /* n >= 7, so r >= 2 */
        for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
            for (pl = pm; pl > (char *) a && CMP(thunk, pl - es, pl) > 0;
                 pl -= es) {
                q_swap(pl, pl - es);
                if (++swap_cnt > r)
                    goto nevermind;
            }
        return;
    }

nevermind:
    nl = (pb - pa) / es;
    nr = (pd - pc) / es;

    if (nl > 100 && nr > 100) {
        struct qsort *q = kmalloc(sizeof(struct qsort), GFP_KERNEL);
        init_qsort(q, a, nl, c);
        queue_work(workqueue, &q->w);
    } else if (nl > 0) {
        qs->a = a;
        qs->n = nl;
        /* The struct qsort is used for recursive call, so don't free it in
         * this iteration.
         */
        do_free = false;
        qsort_algo(w);
    }

    if (nr > 0) {
        a = pn - nr * es;
        n = nr;
        goto top;
    }

    if (do_free)
        kfree(qs);
}

void sort_main(void *sort_buffer, size_t size, size_t es, cmp_t cmp)
{
    /* The allocation must be dynamic so that the pointer can be reliably freed
     * within the work function.
     */
    struct qsort *q = kmalloc(sizeof(struct qsort), GFP_KERNEL);
    struct common common = {
        .swaptype = ((char *) sort_buffer - (char *) 0) % sizeof(long) ||
                            es % sizeof(long)
                        ? 2
                    : es == sizeof(long) ? 0
                                         : 1,
        .es = es,
        .cmp = cmp,
    };
    //printk("swaptype: %d, es: %lu\n", common.swaptype, common.es);

   
    init_qsort(q, sort_buffer, size, &common);

    queue_work(workqueue, &q->w);
   

    /* Ensure completion of all work before proceeding, as reliance on objects
     * allocated on the stack necessitates this. If not, there is a risk of
     * the work item referencing a pointer that has ceased to exist.
     */
    drain_workqueue(workqueue);
}
