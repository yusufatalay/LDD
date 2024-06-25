#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s :" fmt, __func__

/*Device private data structure*/
struct gpiodev_private_data {
  char label[20];
  struct gpio_desc *desc;
};

/*Driver private data structure*/
struct gpiodrv_private_data {
  int total_devices;
  struct class *class_gpio;
  struct device **dev;
};

struct gpiodrv_private_data gpio_drv_data;

/*device attributes*/
ssize_t direction_show(struct device *dev, struct device_attribute *attr,
                       char *buf) {
  struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
  int dir;
  char *direction;

  dir = gpiod_get_direction(dev_data->desc);
  if (dir < 0) {
    return dir;
  }

  direction = dir == 0 ? "out" : "in";

  return sprintf(buf, "%s\n", direction);
}

ssize_t direction_store(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count) {
  int ret = 0;
  struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
  if (sysfs_streq(buf, "in")) {
    ret = gpiod_direction_input(dev_data->desc);
  } else if (sysfs_streq(buf, "out")) {

    ret = gpiod_direction_output(dev_data->desc, 0);
  } else {
    return -EINVAL;
  }

  return ret ?: count;
}

ssize_t value_show(struct device *dev, struct device_attribute *attr,
                   char *buf) {
  struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
  int value = 0;
  value = gpiod_get_value(dev_data->desc);
  return sprintf(buf, "%d\n", value);
}

ssize_t value_store(struct device *dev, struct device_attribute *attr,
                    const char *buf, size_t count) {
  struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
  int ret = 0;
  long value = 0;

  ret = kstrtol(buf, 0, &value);
  if (ret) {
    return ret;
  }

  gpiod_set_value(dev_data->desc, value);

  return count;
}

ssize_t label_show(struct device *dev, struct device_attribute *attr,
                   char *buf) {
  struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
  return sprintf(buf, "%s\n", dev_data->label);
}

static DEVICE_ATTR_RW(direction);
static DEVICE_ATTR_RW(value);
static DEVICE_ATTR_RO(label);

static struct attribute *gpio_attrs[] = {
    &dev_attr_direction.attr, &dev_attr_value.attr, &dev_attr_label.attr, NULL};

static struct attribute_group gpio_attr_group = {.attrs = gpio_attrs};

static const struct attribute_group *gpio_attr_groups[] = {&gpio_attr_group,
                                                           NULL};
/*device attributes/*/

int gpio_sysfs_probe(struct platform_device *pdev) {
  const char *name;
  int i, ret = 0;

  struct device *dev = &pdev->dev;
  /*parent device node*/
  struct device_node *parent = pdev->dev.of_node;
  /*child node will be populated from below macro*/
  struct device_node *child = NULL;

  struct gpiodev_private_data *dev_data = {0};

  gpio_drv_data.total_devices = of_get_child_count(parent);
  if (gpio_drv_data.total_devices == 0) {
    dev_warn(dev, "no devices found\n");
    return -EINVAL;
  }

  dev_info(dev, "total devices found = %d\n", gpio_drv_data.total_devices);

  gpio_drv_data.dev = devm_kzalloc(
      dev, sizeof(struct device *) * gpio_drv_data.total_devices, GFP_KERNEL);
  if (!gpio_drv_data.dev) {
    dev_err(dev, "not enough memory\n");
    return -ENOMEM;
  }

  for_each_available_child_of_node(parent, child) {
    /*obtained the child*/
    dev_data = devm_kzalloc(dev, sizeof(*dev_data), GFP_KERNEL);
    if (!dev_data) {
      dev_err(dev, "cannot allocate memory\n");
      return -ENOMEM;
    }

    /*obtain data from the child*/
    if (of_property_read_string(child, "label", &name)) {
      dev_warn(dev, "missing label information\n");
      snprintf(dev_data->label, sizeof(dev_data->label), "UNKNWNGPIO%d", i);
    } else {
      strcpy(dev_data->label, name);
      dev_info(dev, "GPIO info = %s\n", dev_data->label);
    }

#if 1
    /*use this if kernel version is 5.4*/
    dev_data->desc = devm_fwnode_get_gpiod_from_child(
        dev, "bone", &child->fwnode, GPIOD_ASIS, dev_data->label);

#endif
#if 0
/*use this if kernel version >=6.4*/
    dev_data->desc = devm_fwnode_gpiod_get(dev, &child->fwnode, "bone",
                                           GPIOD_ASIS, dev_data->label);
#endif

    if (IS_ERR(dev_data->desc)) {
      ret = PTR_ERR(dev_data->desc);
      if (ret == -ENOENT) {
        dev_err(dev, "no gpio has been assigned to the requested function\n");
      }
      return ret;
    }

    /*set the GPIO direction to output*/
    ret = gpiod_direction_output(dev_data->desc, 0);
    if (ret) {
      dev_err(dev, "gpio direction set failed\n");
      return ret;
    }

    /*create devices under /sys/class/bone_gpios*/
    gpio_drv_data.dev[i] =
        device_create_with_groups(gpio_drv_data.class_gpio, dev, 0, dev_data,
                                  gpio_attr_groups, dev_data->label);
    if (IS_ERR(gpio_drv_data.dev[i])) {
      dev_err(dev, "cannot create device under /sys/");
      ret = PTR_ERR(gpio_drv_data.dev[i]);

      return ret;
    }

    i++;
  }

  return 0;
}

int gpio_sysfs_remove(struct platform_device *pdev) {
  int i = 0;

  pr_info("remove called\n");
  for (; i < gpio_drv_data.total_devices; i++) {
    device_unregister(gpio_drv_data.dev[i]);
  }

  return 0;
}

struct of_device_id gpio_device_match[] = {
    {.compatible = "org,bone-gpio-sysfs"}, {}};

struct platform_driver gpiosysfs_platform_driver = {
    .probe = gpio_sysfs_probe,
    .remove = gpio_sysfs_remove,
    .driver = {.name = "bone-gpio-sysfs",
               .of_match_table = of_match_ptr(gpio_device_match)}};

int __init gpio_sysfs_init(void) {
#if 0
  /*Activate here if kernel version is >6.3*/ 
  gpio_drv_data.class_gpio = class_create("bone_gpios");
#endif
#if 1
  /*Activate here if kernel version is <6.4*/
  gpio_drv_data.class_gpio = class_create(THIS_MODULE, "bone_gpios");
#endif
  if (IS_ERR(gpio_drv_data.class_gpio)) {
    pr_err("cannot create class");
    return PTR_ERR(gpio_drv_data.class_gpio);
  }

  platform_driver_register(&gpiosysfs_platform_driver);
  pr_info("module loaded successfully");
  return 0;
}

void __exit gpio_sysfs_exit(void) {
  platform_driver_unregister(&gpiosysfs_platform_driver);

  class_destroy(gpio_drv_data.class_gpio);
}

module_init(gpio_sysfs_init);
module_exit(gpio_sysfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yusuf Atalay");
MODULE_DESCRIPTION("A GPIO sysfs driver.");
