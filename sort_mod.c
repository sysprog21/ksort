#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "sort.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Concurrent sorting driver");
MODULE_VERSION("0.1");

#define DEVICE_NAME "sort"

static dev_t dev = -1;
static struct cdev cdev;
static struct class *class;

struct workqueue_struct *workqueue;

static int sort_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int sort_release(struct inode *inode, struct file *file)
{
    return 0;
}

static int num_compare(const void *a, const void *b)
{
    return (*(int *) a - *(int *) b);
}

static ssize_t sort_read(struct file *file,
                         char *buf,
                         size_t size,
                         loff_t *offset)
{
    unsigned long len;
    size_t es;

    void *sort_buffer = kmalloc(size, GFP_KERNEL);
    if (!sort_buffer)
        return 0;

    /* FIXME: Requiring users to manually input data into a buffer for read
     * operations is not ideal, even if it is only for testing purposes.
     */
    len = copy_from_user(sort_buffer, buf, size);
    if (len != 0)
        return 0;

    /* TODO: While currently designed to handle integer arrays, there is an
     * intention to expand the sorting object's capability to accommodate
     * various types in the future.
     */
    es = sizeof(int);
    sort_main(sort_buffer, size / es, es, num_compare);

    len = copy_to_user(buf, sort_buffer, size);
    if (len != 0)
        return 0;

    kfree(sort_buffer);
    return size;
}

static ssize_t sort_write(struct file *file,
                          const char *buf,
                          size_t size,
                          loff_t *offset)
{
    return 0;
}

static const struct file_operations fops = {
    .read = sort_read,
    .write = sort_write,
    .open = sort_open,
    .release = sort_release,
    .owner = THIS_MODULE,
};

static int __init sort_init(void)
{
    struct device *device;

    printk(KERN_INFO DEVICE_NAME ": loaded\n");

    if (alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME) < 0)
        return -1;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    class = class_create(THIS_MODULE, DEVICE_NAME);
#else
    class = class_create(DEVICE_NAME);
#endif
    if (IS_ERR(class)) {
        goto error_unregister_chrdev_region;
    }

    device = device_create(class, NULL, dev, NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        goto error_class_destroy;
    }

    cdev_init(&cdev, &fops);
    if (cdev_add(&cdev, dev, 1) < 0)
        goto error_device_destroy;

    workqueue = alloc_workqueue("sortq", 0, WQ_MAX_ACTIVE);
    if (!workqueue)
        goto error_cdev_del;

    return 0;

error_cdev_del:
    cdev_del(&cdev);
error_device_destroy:
    device_destroy(class, dev);
error_class_destroy:
    class_destroy(class);
error_unregister_chrdev_region:
    unregister_chrdev_region(dev, 1);

    return -1;
}

static void __exit sort_exit(void)
{
    /* Given that drain_workqueue will be executed, there is no need for an
     * explicit flush action.
     */
    destroy_workqueue(workqueue);

    cdev_del(&cdev);
    device_destroy(class, dev);
    class_destroy(class);
    unregister_chrdev_region(dev, 1);

    printk(KERN_INFO DEVICE_NAME ": unloaded\n");
}

module_init(sort_init);
module_exit(sort_exit);
