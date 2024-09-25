#include <linux/ioctl.h>
#include <linux/cdev.h>

struct vvgpu_dev {
	struct semaphore sem;     /* Mutual exclusion */
	struct cdev cdev;
    char var[16];
};

extern struct vvgpu_dev *vvgpu_devices;
