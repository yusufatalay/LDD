#include "platform.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s :" fmt, __func__

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

/*Called when matched platform device is found*/
int pcd_platform_driver_probe(struct platform_device *pdev) {
  int ret = 0;
  struct pcdev_private_data *dev_data;
  struct pcdev_platform_data *pdata;

  pr_info("A device is detected\n");

  /*Get the platform data*/
  pdata = (struct pcdev_platform_data *)dev_get_platdata(&pdev->dev);
  if (!pdata) {
    pr_err("no platform info available\n");
    ret = -EINVAL;
    goto out;
  }

  /*Dynamically allocate memory for the device private data*/
  dev_data = devm_kzalloc(&pdev->dev,sizeof(*dev_data), GFP_KERNEL);
  if (!dev_data) {
    pr_err("cannot allocate memory for device\n");
    ret = -ENOMEM;
    goto out;
  }

  dev_data->pdata.size = pdata->size;
  dev_data->pdata.perm = pdata->perm;
  dev_data->pdata.serial_number = pdata->serial_number;

  pr_info("Device serial number = %s\n", dev_data->pdata.serial_number);
  pr_info("Device size = %d\n", dev_data->pdata.size);
  pr_info("Device permission =  %d\n", dev_data->pdata.perm);

  /*Dynamically allocate memory for the device buffer using size
  information from the platform data*/
  dev_data->buffer = devm_kzalloc(&pdev->dev, dev_data->pdata.size, GFP_KERNEL);
  if (!(dev_data->buffer)) {
    pr_err("cannot allocate memory for device buffer\n");
    ret = -ENOMEM;
    goto dev_data_free;
  }

  /*Save the device private data pointer in platform device structure*/
  dev_set_drvdata(&pdev->dev, dev_data);

  /*Get the device number*/
  dev_data->dev_num = pcdrv_data.device_num_base + pdev->id;

  /*Do cdev init and cdev add*/
  cdev_init(&dev_data->cdev, &pcd_fops);

  dev_data->cdev.owner = THIS_MODULE;
  ret = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
  if (ret < 0) {
    pr_err("cannot add character device");
    goto dev_buffer_free;
  }

  /*Create device file for the detected platform device*/
  pcdrv_data.device_pcd =
      device_create(pcdrv_data.class_pcd, NULL, dev_data->dev_num, NULL,
                    "pcdev-%d", pdev->id);
  if (IS_ERR(pcdrv_data.device_pcd)) {
    pr_err("device create failed");
    ret = PTR_ERR(pcdrv_data.device_pcd);
    goto cdev_del;
  }

  pcdrv_data.total_devices++;
  pr_info("The probe was successful\n");
  return 0;

  /*Error handling*/
cdev_del:
  cdev_del(&dev_data->cdev);
dev_buffer_free:
  devm_kfree(&pdev->dev, dev_data->buffer);
dev_data_free:
  devm_kfree(&pdev->dev,dev_data);
out:
  pr_info("device probe failed\n");
  return ret;
}

/*Called when device is removed from the system*/
int pcd_platform_driver_remove(struct platform_device *pdev) {
  struct pcdev_private_data *dev_data = dev_get_drvdata(&pdev->dev);
  /*Remove a device that was created with device_create()*/
  device_destroy(pcdrv_data.class_pcd, dev_data->dev_num);

  /*Remove a cdev entry from the system*/
  cdev_del(&dev_data->cdev);

  pcdrv_data.total_devices--;
  pr_info("A device is removed\n");
  return 0;
}

struct platform_driver pcd_platform_driver = {
    .probe = pcd_platform_driver_probe,
    .remove = pcd_platform_driver_remove,
    .driver = {.name = "pseudo-char-device"}};

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
  pcdrv_data.class_pcd = class_create("pcd_class");
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
