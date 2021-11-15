#include <linux/ioctl.h>
#include <linux/cdev.h>
#include "../sun_bus/sun_bus.h"

#define SUN_QUANTUM  4000 /* use a quantum size like scull */
#define SUN_QSET     500

#define SUN_DEVS 4 /* sun0 to sun3*/

struct sun_dev {
	void **data;
	struct sun_dev *next;  /* next listitem */
	int vmas;                 /* active mappings */
	int quantum;              /* the current allocation size */
	int qset;                 /* the current array size */
	size_t size;              /* 32-bit will suffice */
	struct semaphore sem;     /* Mutual exclusion */
	struct cdev cdev;
    char devname[20];
    struct sun_device core_dev;
};

extern struct sun_dev *sun_devices;
