/* Acharacter device which returns bytes from a PRNG in the xorshiro family. */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define DEVICE_NAME "xoro"
#define CLASS_NAME "xoro"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Xoroshiro128p PRNG");
MODULE_VERSION("0.1");

static int major_number;
static struct class *dev_class = NULL;
static struct device *dev_device = NULL;

static int n_opens = 0; /* Count the number of times device is opened. */

/* Mutex to allow only one userspace program to read at once. */
static DEFINE_MUTEX(xoro_mutex);

/* This is xoroshiro128+ 1.0, written by David Blackman and Sebastiano Vigna
 * (vigna@acm.org). It passes all tests we are aware of except for the four
 * lower bits, which might fail linearity tests (and just those), so if
 * low linear complexity is not considered an issue (as it is usually the
 * case) it can be used to generate 64-bit outputs, too; moreover, this
 * generator has a very mild Hamming-weight dependency making our test
 * (https://xoshiro.di.unimi.it/hwd.php) fail after 5 TB of output; we believe
 * this slight bias cannot affect any application. If you are concerned,
 * use xoroshiro128++, xoroshiro128** or xoshiro256+.
 *
 * We suggest to use a sign test to extract a random Boolean value, and
 * right shifts to extract subsets of bits.
 *
 * The state must be seeded so that it is not everywhere zero. If you have
 * a 64-bit seed, we suggest to seed a splitmix64 generator and use its
 * output to fill s.
 *
 * NOTE: the parameters (a=24, b=16, b=37) of this version give slightly
 * better results in our test than the 2016 version (a=55, b=14, c=36).
 */

static inline uint64_t rotl(const uint64_t x, int k)
{
    return (x << k) | (x >> (64 - k));
}

static uint64_t s[2];

static void seed(uint64_t s0, uint64_t s1)
{
    s[0] = s0;
    s[1] = s1;
    return;
}

static uint64_t next(void)
{
    const uint64_t s0 = s[0];
    uint64_t s1 = s[1];
    const uint64_t result = s0 + s1;

    s1 ^= s0;
    s[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16); /* a, b */
    s[1] = rotl(s1, 37);                   /* c */

    return result;
}

/* This is the jump function for the generator. It is equivalent to 2^64 calls
 * to next(); it can be used to generate 2^64 non-overlapping subsequences for
 * parallel computations.
 */
static void jump(void)
{
    static const uint64_t JUMP[] = {0xdf900294d8f554a5, 0x170865df4b3201fc};

    uint64_t s0 = 0;
    uint64_t s1 = 0;
    int i, b;
    for (i = 0; i < sizeof JUMP / sizeof *JUMP; i++)
        for (b = 0; b < 64; b++) {
            if (JUMP[i] & (uint64_t) (1) << b) {
                s0 ^= s[0];
                s1 ^= s[1];
            }
            next();
        }

    s[0] = s0;
    s[1] = s1;
}

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .release = dev_release,
};

/**
 * Initialize /dev/xoro.
 * Returns 0 if successful.
 */
static int __init xoro_init(void)
{
    printk(KERN_INFO "XORO: Initializing...\n");

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (0 > major_number) {
        printk(KERN_ALERT "XORO: Failed to register major_number\n");
        return major_number;
    }
    printk(KERN_INFO "XORO:   major_number=%d\n", major_number);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    dev_class = class_create(THIS_MODULE, DEVICE_NAME);
#else
    dev_class = class_create(DEVICE_NAME);
#endif
    if (IS_ERR(dev_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);  // backout
        printk(KERN_ALERT "XORO: Failed to create dev_class\n");
        return PTR_ERR(dev_class);
    }
    printk(KERN_INFO "XORO:   dev_class[name]=%s\n", CLASS_NAME);

    // Register the device driver
    dev_device = device_create(dev_class, NULL, MKDEV(major_number, 0), NULL,
                               DEVICE_NAME);
    if (IS_ERR(dev_device)) {
        class_destroy(dev_class);                      // backout
        unregister_chrdev(major_number, DEVICE_NAME);  // backout
        printk(KERN_ALERT "XORO: Failed to create dev_device\n");
        return PTR_ERR(dev_device);
    }
    printk(KERN_INFO "XORO:   dev_device[name]=%s\n", DEVICE_NAME);

    mutex_init(&xoro_mutex);

    seed(314159265, 1618033989);  // Initialize PRNG with pi and phi.

    printk(KERN_INFO "XORO:   Initialized\n");
    return 0;
}

/**
 * Free all module resources.
 * Not used if part of a built-in driver rather than a LKM.
 */
static void __exit xoro_exit(void)
{
    mutex_destroy(&xoro_mutex);

    device_destroy(dev_class, MKDEV(major_number, 0));

    class_unregister(dev_class);
    class_destroy(dev_class);

    unregister_chrdev(major_number, DEVICE_NAME);

    printk(KERN_INFO "XORO: Exit\n");
}

/**
 * open() syscall.
 * Increment counter, perform another jump to effectively give each
 * reader a separate PRNG.
 * @inodep Pointer to an inode object (defined in linux/fs.h)
 * @filep Pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep)
{
    /* Try to acquire the mutex (returns 0 on fail) */
    if (!mutex_trylock(&xoro_mutex)) {
        printk(KERN_INFO "XORO: %s busy\n", DEVICE_NAME);
        return -EBUSY;
    }

    jump();  // xorolus.c

    printk(KERN_INFO "XORO: %s opened. n_opens=%d\n", DEVICE_NAME, n_opens++);

    return 0;
}

/**
 * Called whenever device is read from user space.
 * @filep Pointer to a file object (defined in linux/fs.h).
 * @buffer Pointer to the buffer to which this function may write data.
 * @len Number of bytes requested.
 * @offset Unused.
 * Returns number of bytes successfully read. Negative on error.
 */
static ssize_t dev_read(struct file *filep,
                        char *buffer,
                        size_t len,
                        loff_t *offset)
{
    // Give at most 8 bytes per read.
    size_t len_ = (len > 8) ? 8 : len;

    uint64_t value = next();  // xorolus.c

    // copy_to_user has the format ( * to, *from, size) and returns 0 on success
    int n_notcopied = copy_to_user(buffer, (char *) (&value), len_);

    if (0 != n_notcopied) {
        printk(KERN_ALERT "XORO: Failed to read %d/%ld bytes\n", n_notcopied,
               len_);
        return -EFAULT;
    } else {
        printk(KERN_INFO "XORO: read %ld bytes\n", len_);
        return len_;
    }
}

/**
 * Called when the userspace program calls close().
 * @inodep A pointer to an inode object (defined in linux/fs.h)
 * @filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep)
{
    mutex_unlock(&xoro_mutex);
    printk(KERN_INFO "XORO: %s closed\n", DEVICE_NAME);
    return 0;
}

module_init(xoro_init);
module_exit(xoro_exit);
