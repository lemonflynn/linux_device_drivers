#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include "sun_bus.h"

MODULE_AUTHOR("flynn");
MODULE_LICENSE("Dual BSD/GPL");
static char *Version = "$Revision: 0.1 $";

static void sun_bus_release(struct device *dev)
{
    printk(KERN_DEBUG "sunbus released\n");
}

static int sun_bus_match(struct device *dev, struct device_driver *driver)
{
    return !strncmp(dev_name(dev), driver->name, strlen(driver->name));
}

static int sun_bus_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	if (add_uevent_var(env, "SUNBUS_VERSION=%s", Version))
		return -ENOMEM;

    return 0;
}

static ssize_t version_show(struct bus_type *bus, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", Version);
}

static BUS_ATTR_RO(version);

struct device sun_bus = {
    .release = sun_bus_release,
};

struct bus_type sun_bus_type = {
    .name = "sun",
    .match = sun_bus_match,
    .uevent = sun_bus_uevent,
};

/*
 * For now, no references to LDDbus devices go out which are not
 * tracked via the module reference count, so we use a no-op
 * release function.
 */
static void sun_dev_release(struct device *dev)
{ }

int register_sun_device(struct sun_device *sun_dev)
{
    sun_dev->dev.bus = &sun_bus_type;
    sun_dev->dev.parent = &sun_bus;
    sun_dev->dev.release = sun_dev_release;
    dev_set_name(&sun_dev->dev, sun_dev->name);

    return device_register(&sun_dev->dev);
}
void unregister_sun_device(struct sun_device *sun_dev)
{
    device_unregister(&sun_dev->dev);
}

EXPORT_SYMBOL(register_sun_device);
EXPORT_SYMBOL(unregister_sun_device);

static ssize_t show_version(struct device_driver *driver, char *buf)
{
    struct sun_driver *sun_drv = to_sun_driver(driver);
    sprintf(buf, "%s\n", sun_drv->version);
    return strlen(buf);
}

int register_sun_driver(struct sun_driver *driver)
{
    int ret;
    
    driver->driver.bus = &sun_bus_type;
    ret = driver_register(&driver->driver);
    if(ret)
        return ret;
    driver->version_attr.attr.name = "version";
    driver->version_attr.attr.mode = S_IRUGO;
    driver->version_attr.show = show_version;
    driver->version_attr.store = NULL;
    return driver_create_file(&driver->driver, &driver->version_attr);
}
void unregister_sun_driver(struct sun_driver *driver)
{
    driver_unregister(&driver->driver);
}

EXPORT_SYMBOL(register_sun_driver);
EXPORT_SYMBOL(unregister_sun_driver);

static int __init sun_bus_init(void)
{
    int ret;

    ret = bus_register(&sun_bus_type);
    if (ret) {
		printk(KERN_ERR "Unable to register sun bus, failure was %d\n",ret);
        return ret;
    }
    if (bus_create_file(&sun_bus_type, &bus_attr_version))
        printk(KERN_NOTICE "Unable to create version attribute\n");
    dev_set_name(&sun_bus,"sun0");
    ret = device_register(&sun_bus);
    if (ret) {
        printk(KERN_NOTICE "Unable to register sun0\n");
        bus_unregister(&sun_bus_type);
    }
    return ret;
}

static void sun_bus_exit(void)
{
    device_unregister(&sun_bus);
    bus_unregister(&sun_bus_type);
}

module_init(sun_bus_init);
module_exit(sun_bus_exit);
