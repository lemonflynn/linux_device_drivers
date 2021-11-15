extern struct bus_type sun_bus_type;

/*
struct device_driver {
    char *name;
    struct bus_type *bus;
    struct kobject kobj;
    struct list_head devices;
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    int (*shutdown)(struct device *dev);
};

struct device {
    struct device *parent;
    struct kobject kobj:
    //A string that uniquely identifier this device on the bus
    char bus_id[BUS_ID_SIZE];
    //Identifier which kind of bus this device sit on
    struct bus_type *bus;
    struct device_driver *driver;
    void *driver_data;
    void(*release)(struct device *dev);
    ...
};
*/

struct sun_driver {
    char *version;
    struct module *module;
    struct device_driver driver;
    struct driver_attribute version_attr;
};

#define to_sun_driver(drv) container_of(drv, struct sun_driver, driver);
struct sun_device {
    char *name;
    struct sun_driver *driver;
    struct device dev;
};

#define to_sun_device(dev) container_of(dev, struct sun_device, dev)

extern int register_sun_driver(struct sun_driver *driver);
extern void unregister_sun_driver(struct sun_driver *driver);
extern int register_sun_device(struct sun_device *sun_dev);
extern void unregister_sun_device(struct sun_device *sun_dev);
