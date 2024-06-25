#ifndef PCD_PLATFORM_DRIVER_DEVICE_TREE_SYSFS_H
#define PCD_PLATFORM_DRIVER_DEVICE_TREE_SYSFS_H
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

int check_permission(int dev_perm, int acc_mode);
loff_t pcd_llseek(struct file *filep, loff_t offset, int whence);

ssize_t pcd_read(struct file *filep, char __user *buff, size_t count,
                 loff_t *f_pos);

ssize_t pcd_write(struct file *filep, const char __user *buff, size_t count,
                  loff_t *f_pos);

int pcd_open(struct inode *inode, struct file *filep);

int pcd_release(struct inode *inode, struct file *filep);


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


#endif
