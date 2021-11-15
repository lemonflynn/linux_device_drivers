#include<linux/module.h>
#include<linux/kernel.h>
#include <linux/slab.h>
#include<linux/fs.h>
#include <linux/uaccess.h>
#include "sun_char.h"

static dev_t ndev;
int sun_major =     0;
int sun_devs =      SUN_DEVS;
int sun_qset =      SUN_QSET;
int sun_quantum =   SUN_QUANTUM;
struct sun_dev *sun_devices;

static struct sun_driver sun_char_driver = { 
    .version = "$Revision: 0.1 $",
    .module = THIS_MODULE,
    .driver = { 
        .name = "sun",
    }   
};

static int sun_trim(struct sun_dev *dev);

static ssize_t sun_show_dev(struct device *ddev, struct device_attribute *attr, char *buf)
{
    struct sun_dev * dev = ddev->driver_data;

    return print_dev_t(buf, dev->cdev.dev);
}
    
static DEVICE_ATTR(dev, S_IRUGO, sun_show_dev, NULL);

static void sunchar_register_dev(struct sun_dev *dev, int index)
{
    sprintf(dev->devname, "sun_char%d", index);
    dev->core_dev.name = dev->devname;
    dev->core_dev.driver = &sun_char_driver;
    dev->core_dev.dev.driver_data = dev;
    register_sun_device(&dev->core_dev);
    device_create_file(&dev->core_dev.dev, &dev_attr_dev);
}

static int sun_open(struct inode *inode, struct file *filp)
{
    struct sun_dev *dev; /* device information */

    /*  Find the device */
    dev = container_of(inode->i_cdev, struct sun_dev, cdev);

    /* now trim to 0 the length of the device if open was write-only */
    if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (down_interruptible (&dev->sem))
            return -ERESTARTSYS;
        sun_trim(dev); /* ignore errors */
        up (&dev->sem);
    }

    /* and use filp->private_data to point to the device data */
    filp->private_data = dev;

    return 0;/* success */
}

struct sun_dev * sun_follow(struct sun_dev * dev, int n)
{
   while (n--) {
        if (!dev->next) {
            dev->next = kmalloc(sizeof(struct sun_dev), GFP_KERNEL);
            memset(dev->next, 0, sizeof(struct sun_dev));
        }
        dev = dev->next;
    }
    return dev;
}

static ssize_t sun_read(struct file *filp, char __user *buffer, size_t count, loff_t *f_pos)
{
    struct sun_dev *dev = filp->private_data; /* the first listitem */
    struct sun_dev *dptr;
    int quantum = dev->quantum;
    int qset = dev->qset;
    int itemsize = quantum * qset; /* how many bytes in the listitem */
    int item, s_pos, q_pos, rest;
    ssize_t retval = 0;

    if (down_interruptible (&dev->sem))
        return -ERESTARTSYS;
    if (*f_pos > dev->size) 
        goto nothing;
    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;
    /* find listitem, qset index, and offset in the quantum */
    item = ((long) *f_pos) / itemsize;
    rest = ((long) *f_pos) % itemsize;
    s_pos = rest / quantum; q_pos = rest % quantum;

        /* follow the list up to the right position (defined elsewhere) */
    dptr = sun_follow(dev, item);

    if (!dptr->data)
        goto nothing; /* don't fill holes */
    if (!dptr->data[s_pos])
        goto nothing;
    if (count > quantum - q_pos)
        count = quantum - q_pos; /* read only up to the end of this quantum */

    if (copy_to_user (buffer, dptr->data[s_pos] + q_pos, count)) {
        retval = -EFAULT;
        goto nothing;
    }   
    up (&dev->sem);

    *f_pos += count;
    return count;

nothing:
    up (&dev->sem);
    return retval;
}

static ssize_t sun_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos)
{
    struct sun_dev *dev = filp->private_data;
    struct sun_dev *dptr;
    int quantum = dev->quantum;
    int qset = dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;
    ssize_t retval = -ENOMEM; /* our most likely error */

    if (down_interruptible (&dev->sem))
        return -ERESTARTSYS;

    /* find listitem, qset index and offset in the quantum */
    item = ((long) *f_pos) / itemsize;
    rest = ((long) *f_pos) % itemsize;
    s_pos = rest / quantum; q_pos = rest % quantum;

    /* follow the list up to the right position */
    dptr = sun_follow(dev, item);
    if (!dptr->data) {
        dptr->data = kmalloc(qset * sizeof(void *), GFP_KERNEL);
        if (!dptr->data)
            goto nomem;
        memset(dptr->data, 0, qset * sizeof(char *));
    }

    if (!dptr->data[s_pos]) {
        dptr->data[s_pos] = kmalloc(sun_quantum, GFP_KERNEL);
        if (!dptr->data[s_pos])
            goto nomem;
        memset(dptr->data[s_pos], 0, sun_quantum);
    }
    if (count > quantum - q_pos)
        count = quantum - q_pos; /* write only up to the end of this quantum */
    if (copy_from_user (dptr->data[s_pos]+q_pos, buffer, count)) {
        retval = -EFAULT;
        goto nomem;
    }
    *f_pos += count;

    /* update the size */
    if (dev->size < *f_pos)
        dev->size = *f_pos;
    up (&dev->sem);
    return count;

nomem:
    up (&dev->sem);
    return retval;
}

struct file_operations sun_fops =
{
	.owner	=	THIS_MODULE,
	.open	=	sun_open,
	.read	=	sun_read,
	.write	= 	sun_write,
};

int sun_trim(struct sun_dev *dev)
{
    struct sun_dev *next, *dptr;
    int qset = dev->qset;   /* "dev" is not-null */
    int i;

    if (dev->vmas) /* don't trim: there are active mappings */
        return -EBUSY;

    for (dptr = dev; dptr; dptr = next) { /* all the list items */
        if (dptr->data) {
            for (i = 0; i < qset; i++)
                if (dptr->data[i])
                    kfree(dptr->data[i]);

            kfree(dptr->data);
            dptr->data=NULL;
        }
        next=dptr->next;
        if (dptr != dev) kfree(dptr); /* all of them but the first */
    }
    dev->size = 0;
    dev->qset = sun_qset;
    dev->quantum = sun_quantum;
    dev->next = NULL;
    return 0;
}

static void sun_setup_cdev(struct sun_dev *dev, int index)
{
    int err, devno = MKDEV(sun_major, index);

    cdev_init(&dev->cdev, &sun_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &sun_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    /* Fail gracefully if need be */
    if (err)
        printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

static int sun_init(void)
{
	int ret, i;
	ret = alloc_chrdev_region(&ndev, 0, sun_devs, "sun_char");
	if(ret < 0)
		return ret;
	printk("sun init major=%d, minor=%d\n", MAJOR(ndev), MINOR(ndev));
    sun_major = MAJOR(ndev);

    /*
    * Register with the driver core.
    */
    register_sun_driver(&sun_char_driver);
    sun_devices = kmalloc(sun_devs*sizeof (struct sun_dev), GFP_KERNEL);
    if (!sun_devices) {
        ret = -ENOMEM;
        goto fail_malloc;
    }   
    memset(sun_devices, 0, sun_devs*sizeof (struct sun_dev));
    for (i = 0; i < sun_devs; i++) {
        sun_devices[i].quantum = sun_quantum;
        sun_devices[i].qset = sun_qset;
        sema_init (&sun_devices[i].sem, 1); 
        sun_setup_cdev(sun_devices + i, i); 
        sunchar_register_dev(sun_devices + i, i);
    }   

    return 0; /* succeed */

fail_malloc:
    unregister_chrdev_region(ndev, sun_devs);
    return ret;
}		

static void sun_exit(void)
{
    int i;
    for (i = 0; i < sun_devs; i++) {
        unregister_sun_device(&sun_devices[i].core_dev);
        cdev_del(&sun_devices[i].cdev);
        sun_trim(sun_devices + i);
    }
    kfree(sun_devices);

    unregister_sun_driver(&sun_char_driver);
    unregister_chrdev_region(MKDEV(sun_major, 0), sun_devs);
	printk("Removing sun_char module..\n");
}

module_init(sun_init);
module_exit(sun_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("flynn");
MODULE_DESCRIPTION("A char device driver..");
