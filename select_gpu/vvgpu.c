#include<linux/module.h>
#include<linux/kernel.h>
#include <linux/slab.h>
#include<linux/fs.h>
#include <linux/uaccess.h>
#include "vvgpu.h"

static dev_t ndev;
int vvgpu_major =     0;
int vvgpu_devs =      1;
int var_len = sizeof("DISPLAY=:0.0");
struct vvgpu_dev *vvgpu_devices;

static int vvgpu_open(struct inode *inode, struct file *filp)
{
    struct vvgpu_dev *dev; /* device information */

    /*  Find the device */
    dev = container_of(inode->i_cdev, struct vvgpu_dev, cdev);

    filp->private_data = dev;
    return 0;/* success */
}

static ssize_t vvgpu_read(struct file *filp, char __user *buffer, size_t count, loff_t *f_pos)
{
    struct vvgpu_dev *dev = filp->private_data; /* the first listitem */
    ssize_t retval = 0;

    if (down_interruptible (&dev->sem))
        return -ERESTARTSYS;

    if (copy_to_user (buffer, dev->var, var_len)) {
        retval = -EFAULT;
        goto nothing;
    }   
    if(dev->var[var_len - 2] == '0')
        dev->var[var_len - 2] = '1';
    else
        dev->var[var_len - 2] = '0';

    up (&dev->sem);

    return var_len;

nothing:
    up (&dev->sem);
    return retval;
}

struct file_operations vvgpu_fops =
{
	.owner	=	THIS_MODULE,
	.open	=	vvgpu_open,
	.read	=	vvgpu_read,
	.write	= 	NULL,
};

static void vvgpu_setup_cdev(struct vvgpu_dev *dev, int index)
{
    int err, devno = MKDEV(vvgpu_major, index);

    cdev_init(&dev->cdev, &vvgpu_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &vvgpu_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    /* Fail gracefully if need be */
    if (err)
        printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

static int vvgpu_init(void)
{
	int ret, i;
	ret = alloc_chrdev_region(&ndev, 0, vvgpu_devs, "vvgpu_char");
	if(ret < 0)
		return ret;
	printk("vvgpu init major=%d, minor=%d\n", MAJOR(ndev), MINOR(ndev));
    vvgpu_major = MAJOR(ndev);

    vvgpu_devices = kmalloc(vvgpu_devs*sizeof (struct vvgpu_dev), GFP_KERNEL);
    if (!vvgpu_devices) {
        ret = -ENOMEM;
        goto fail_malloc;
    }   
    memset(vvgpu_devices, 0, vvgpu_devs*sizeof (struct vvgpu_dev));
    for (i = 0; i < vvgpu_devs; i++) {
        sema_init(&vvgpu_devices[i].sem, 1); 
        memcpy(vvgpu_devices[i].var, "DISPLAY=:0.0", var_len);
        vvgpu_setup_cdev(vvgpu_devices + i, i); 
    }   

    return 0; /* succeed */

fail_malloc:
    unregister_chrdev_region(ndev, vvgpu_devs);
    return ret;
}		

static void vvgpu_exit(void)
{
    int i;
    for (i = 0; i < vvgpu_devs; i++) {
        cdev_del(&vvgpu_devices[i].cdev);
    }
    kfree(vvgpu_devices);

    unregister_chrdev_region(MKDEV(vvgpu_major, 0), vvgpu_devs);
	printk("Removing vvgpu_char module..\n");
}

module_init(vvgpu_init);
module_exit(vvgpu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xulin");
MODULE_DESCRIPTION("Select a GPU..");
