#include "platform.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s :" fmt, __func__

struct device_config {
  int config_item1;
  int config_item2;
};

enum pcdev_names {
  PCDEVA1X,
  PCDEVB1X,
  PCDEVC1X,
  PCDEVD1X,
};

struct device_config pcdev_config[] = {
    [PCDEVA1X] = {.config_item1 = 60, .config_item2 = 120},
    [PCDEVB1X] = {.config_item1 = 2, .config_item2 = 3},
    [PCDEVC1X] = {.config_item1 = 99, .config_item2 = 9},
    [PCDEVD1X] = {.config_item1 = 12, .config_item2 = 120},
};

// used for device name matching
struct platform_device_id pcdevs_ids[] = {
    [0] = {.name = "pcdev-A1X", .driver_data = PCDEVA1X},
    [1] = {.name = "pcdev-B1X", .driver_data = PCDEVB1X},
    [2] = {.name = "pcdev-C1X", .driver_data = PCDEVC1X},
    [3] = {.name = "pcdev-D1X", .driver_data = PCDEVD1X},
    {} /*Null termination*/
};

// Used for 'open format' matching
struct of_device_id org_pcdev_dt_match[] = {
    [0] = {.compatible = "pcdev-A1X", .data = (void *)PCDEVA1X},
    [1] = {.compatible = "pcdev-B1X", .data = (void *)PCDEVB1X},
    [2] = {.compatible = "pcdev-C1X", .data = (void *)PCDEVC1X},
    [3] = {.compatible = "pcdev-D1X", .data = (void *)PCDEVD1X},
    {} /*Null termination*/
};

int pcd_platform_driver_probe(struct platform_device *pdev);
int pcd_platform_driver_remove(struct platform_device *pdev);

struct platform_driver pcd_platform_driver = {
    .probe = pcd_platform_driver_probe,
    .remove = pcd_platform_driver_remove,
    .id_table = pcdevs_ids,
    .driver = {.name = "pseudo-char-device",
               .of_match_table = org_pcdev_dt_match}};

/*Device private data structure*/
struct pcdev_private_data {
  struct pcdev_platform_data pdata;
  char *buffer;
  dev_t dev_num;
  struct cdev cdev;
};

/*Driver private data structure*/
struct pcdrv_private_data {
  int total_devices;
  dev_t device_num_base;
  struct class *class_pcd;
  struct device *device_pcd;
};

struct pcdrv_private_data pcdrv_data;

int check_permission(int dev_perm, int acc_mode) {
  if (dev_perm == RDWR) {
    return 0;
  }

  // Ensure read only actions
  if ((dev_perm == RDONLY) &&
      ((acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE))) {
    return 0;
  }

  // Ensure write only actions
  if ((dev_perm == WRONLY) &&
      ((acc_mode & FMODE_WRITE) && !(acc_mode & FMODE_READ))) {
    return 0;
  }

  return -EPERM;
}

loff_t pcd_llseek(struct file *filep, loff_t offset, int whence) { return 0; }

ssize_t pcd_read(struct file *filep, char __user *buff, size_t count,
                 loff_t *f_pos) {
  return 0;
}

ssize_t pcd_write(struct file *filep, const char __user *buff, size_t count,
                  loff_t *f_pos) {
  return -ENOMEM;
}

int pcd_open(struct inode *inode, struct file *filep) { return 0; }

int pcd_release(struct inode *inode, struct file *filep) {
  pr_info("release was successful\n");
  return 0;
}

/*file ops of the driver*/
struct file_operations pcd_fops = {.open = pcd_open,
                                   .write = pcd_write,
                                   .read = pcd_read,
                                   .llseek = pcd_llseek,
                                   .release = pcd_release,
                                   .owner = THIS_MODULE};

struct pcdev_platform_data *
pcdev_get_platform_data_from_dt(struct device *dev) {
  struct device_node *dev_node = dev->of_node;
  struct pcdev_platform_data *pdata = {0};

  // if dev_node is not null then this device is instantiated from a device tree
  if (!dev_node) {
    /*device tree is not used*/
    return NULL;
  }

  pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
  if (pdata == NULL) {
    dev_info(dev, "cannot allocate memory for platform data\n");
    return ERR_PTR(-ENOMEM);
  }

  if (of_property_read_string(dev_node, "org,device-serial-num",
                              &pdata->serial_number)) {
    dev_info(dev, "missing serial number property");
    return ERR_PTR(-EINVAL);
  }

  if (of_property_read_u32(dev_node, "org,size", &pdata->size)) {
    dev_info(dev, "missing size property");
    return ERR_PTR(-EINVAL);
  }

  if (of_property_read_u32(dev_node, "org,perm", &pdata->perm)) {
    dev_info(dev, "missing permission property");
    return ERR_PTR(-EINVAL);
  }

  return pdata;
}

/*Called when matched platform device is found*/
int pcd_platform_driver_probe(struct platform_device *pdev) {
  int ret = 0;

  struct pcdev_private_data *dev_data = {0};
  struct pcdev_platform_data *pdata = {0};
  struct device *dev = &pdev->dev;
  int driver_data = 0;

  dev_info(dev, "A device is detected\n");

  /*Get the platform data from device instantiated with device tree*/
  pdata = pcdev_get_platform_data_from_dt(dev);
  if (IS_ERR(pdata)) {
    return PTR_ERR(pdata);
  }

  if (!pdata) {
    /*Get the platform data from device instantiated with device setup code*/
    pdata = (struct pcdev_platform_data *)dev_get_platdata(dev);
    if (!pdata) {
      dev_err(dev, "no platform info available\n");
      return -EINVAL;
    }
    driver_data = pdev->id_entry->driver_data;
  } else {
    driver_data = (int)of_device_get_match_data(dev);
  }
  /*Dynamically allocate memory for the device private data*/
  dev_data = devm_kzalloc(dev, sizeof(*dev_data), GFP_KERNEL);
  if (!dev_data) {
    dev_err(dev, "cannot allocate memory for device\n");
    return -ENOMEM;
  }

  dev_data->pdata.size = pdata->size;
  dev_data->pdata.perm = pdata->perm;
  dev_data->pdata.serial_number = pdata->serial_number;

  pr_info("Device serial number = %s\n", dev_data->pdata.serial_number);
  pr_info("Device size = %d\n", dev_data->pdata.size);
  pr_info("Device permission =  %d\n", dev_data->pdata.perm);

  pr_info("config item 1 = %d \n", pcdev_config[driver_data].config_item1);
  pr_info("config item 2 = %d \n", pcdev_config[driver_data].config_item2);

  /*Dynamically allocate memory for the device buffer using size
  information from the platform data*/
  dev_data->buffer = devm_kzalloc(dev, dev_data->pdata.size, GFP_KERNEL);
  if (!(dev_data->buffer)) {
    dev_err(dev, "cannot allocate memory for device buffer\n");
    return -ENOMEM;
  }

  /*Save the device private data pointer in platform device structure*/
  dev_set_drvdata(dev, dev_data);

  /*Get the device number*/
  dev_data->dev_num = pcdrv_data.device_num_base + pcdrv_data.total_devices;

  /*Do cdev init and cdev add*/
  cdev_init(&dev_data->cdev, &pcd_fops);

  dev_data->cdev.owner = THIS_MODULE;
  ret = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
  if (ret < 0) {
    dev_err(dev, "cannot add character device");
    return ret;
  }

  /*Create device file for the detected platform device*/
  pcdrv_data.device_pcd =
      device_create(pcdrv_data.class_pcd, dev, dev_data->dev_num, NULL,
                    "pcdev-%d", pcdrv_data.total_devices);
  if (IS_ERR(pcdrv_data.device_pcd)) {
    dev_err(dev, "device create failed");
    ret = PTR_ERR(pcdrv_data.device_pcd);
    cdev_del(&dev_data->cdev);
  }

  pcdrv_data.total_devices++;
  dev_info(dev, "The probe was successful\n");
  return 0;
}

/*Called when device is removed from the system*/
int pcd_platform_driver_remove(struct platform_device *pdev) {
  struct device *dev = &pdev->dev;
  struct pcdev_private_data *dev_data = dev_get_drvdata(dev);
  /*Remove a device that was created with device_create()*/
  device_destroy(pcdrv_data.class_pcd, dev_data->dev_num);

  /*Remove a cdev entry from the system*/
  cdev_del(&dev_data->cdev);

  pcdrv_data.total_devices--;
  dev_info(dev, "A device is removed\n");
  return 0;
}

#define MAX_DEVICES 10

static int __init pcd_driver_init(void) {
  int ret = 0;
  /*Dynamically allocate a device number for MAX_DEVICE*/
  ret = alloc_chrdev_region(&pcdrv_data.device_num_base, 0, MAX_DEVICES,
                            "pcdevs");
  if (ret < 0) {
    pr_err("alloc chrdev failed\n");
    return ret;
  }

  /*Create device class under /sys/class*/
  /*NOTE: If kernel version < 6.4  add THIS_MODULE as the first param to the
   * class_create*/
  pcdrv_data.class_pcd = class_create(THIS_MODULE, "pcd_class");
  if (IS_ERR(pcdrv_data.class_pcd)) {
    pr_err("class creation failed\n");
    ret = PTR_ERR(pcdrv_data.class_pcd);
    unregister_chrdev_region(pcdrv_data.device_num_base, MAX_DEVICES);
    return ret;
  }

  /*Register a platform driver*/
  ret = platform_driver_register(&pcd_platform_driver);
  if (ret < 0) {
    pr_info("pcd platform driver failed to load\n");
    unregister_chrdev_region(pcdrv_data.device_num_base, MAX_DEVICES);
    class_destroy(pcdrv_data.class_pcd);
  }
  pr_info("pcd platform driver loaded\n");

  return 0;
}

static void __exit pcd_driver_cleanup(void) {
  /*Unregister the platform driver*/
  platform_driver_unregister(&pcd_platform_driver);

  /*Class destroy*/
  class_destroy(pcdrv_data.class_pcd);

  /*Unregister device numbers for MAX_DEVICES*/
  unregister_chrdev_region(pcdrv_data.device_num_base, MAX_DEVICES);

  pr_info("pcd platform driver unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yusuf Atalay");
MODULE_DESCRIPTION("A pseudo character device driver which handles 4 devices");
