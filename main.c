#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "sort_impl.h"

#define DEVICE_NAME "xoroshiro128p"
#define CLASS_NAME "xoro"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("sorting implementation");
MODULE_VERSION("0.1");

extern void seed(uint64_t, uint64_t);
extern void jump(void);
extern uint64_t next(void);

static int major_number;
static struct class *dev_class = NULL;
static struct device *dev_device = NULL;

/* Count the number of times device is opened */
static int n_opens = 0;

/* Mutex to allow only one userspace program to read at once */
static DEFINE_MUTEX(xoroshiro128p_mutex);

/**
 * Devices are represented as file structure in the kernel.
 */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .release = dev_release,
};

#define TEST_LEN 1000

static int __init cmpint(const void *a, const void *b)
{
    return *(int *) a - *(int *) b;
}

/** @brief Initialize /dev/xoroshiro128p.
 *  @return Returns 0 if successful.
 */
static int __init xoro_init(void)
{
    int *a, i, r = 1, err = -ENOMEM;
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (0 > major_number) {
        printk(KERN_ALERT "XORO: Failed to register major_number\n");
        return major_number;
    }

    dev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(dev_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "XORO: Failed to create dev_class\n");
        return PTR_ERR(dev_class);
    }

    dev_device = device_create(dev_class, NULL, MKDEV(major_number, 0), NULL,
                               DEVICE_NAME);
    if (IS_ERR(dev_device)) {
        class_destroy(dev_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "XORO: Failed to create dev_device\n");
        return PTR_ERR(dev_device);
    }

    mutex_init(&xoroshiro128p_mutex);

    seed(314159265, 1618033989);  // Initialize PRNG with pi and phi.

    a = kmalloc_array(TEST_LEN, sizeof(*a), GFP_KERNEL);
    if (!a)
        return err;

    for (i = 0; i < TEST_LEN; i++) {
        r = (r * 725861) % 6599;
        a[i] = r;
    }

    sort_impl(a, TEST_LEN, sizeof(*a), cmpint, NULL);

    err = -EINVAL;
    for (i = 0; i < TEST_LEN - 1; i++)
        if (a[i] > a[i + 1]) {
            pr_err("test has failed\n");
            goto exit;
        }
    err = 0;
    pr_info("test passed\n");
exit:
    kfree(a);
    return err;
}

/** @brief Free all module resources.
 *         Not used if part of a built-in driver rather than a LKM.
 */
static void __exit xoro_exit(void)
{
    mutex_destroy(&xoroshiro128p_mutex);

    device_destroy(dev_class, MKDEV(major_number, 0));

    class_unregister(dev_class);
    class_destroy(dev_class);

    unregister_chrdev(major_number, DEVICE_NAME);
}

/** @brief open() syscall.
 *         Increment counter, perform another jump to effectively give each
 *         reader a separate PRNG.
 *  @param inodep Pointer to an inode object (defined in linux/fs.h)
 *  @param filep Pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep)
{
    /* Try to acquire the mutex (returns 0 on fail) */
    if (!mutex_trylock(&xoroshiro128p_mutex)) {
        printk(KERN_INFO "XORO: %s busy\n", DEVICE_NAME);
        return -EBUSY;
    }

    jump(); /* in xoroshiro128plus.c */

    printk(KERN_INFO "XORO: %s opened. n_opens=%d\n", DEVICE_NAME, n_opens++);

    return 0;
}

/** @brief Called whenever device is read from user space.
 *  @param filep Pointer to a file object (defined in linux/fs.h).
 *  @param buffer Pointer to the buffer to which this function may write data.
 *  @param len Number of bytes requested.
 *  @param offset Unused.
 *  @return Returns number of bytes successfully read. Negative on error.
 */
static ssize_t dev_read(struct file *filep,
                        char *buffer,
                        size_t len,
                        loff_t *offset)
{
    /* Give at most 8 bytes per read */
    size_t len_ = (len > 8) ? 8 : len;

    uint64_t value = next(); /* in xoroshiro128plus.c */

    /* copy_to_user has the format ( * to, *from, size) and ret 0 on success */
    int n_notcopied = copy_to_user(buffer, (char *) (&value), len_);

    if (0 != n_notcopied) {
        printk(KERN_ALERT "XORO: Failed to read %d/%ld bytes\n", n_notcopied,
               len_);
        return -EFAULT;
    }
    printk(KERN_INFO "XORO: read %ld bytes\n", len_);
    return len_;
}

/** @brief Called when the userspace program calls close().
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep)
{
    mutex_unlock(&xoroshiro128p_mutex);
    return 0;
}

module_init(xoro_init);
module_exit(xoro_exit);
