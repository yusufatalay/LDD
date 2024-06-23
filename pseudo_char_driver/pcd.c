#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s :" fmt, __func__

#define DEV_MEM_SIZE 512

/*pseudo device's memory*/
char device_buffer[DEV_MEM_SIZE];

dev_t device_number;

/*Cdev variable*/
struct cdev pcd_cdev;

loff_t pcd_llseek(struct file *filep, loff_t offset, int whence) {
  pr_info("lseek requested\n");

  loff_t temp = 0;
  pr_info("current file position %lld\n", filep->f_pos);

  switch (whence) {
  case SEEK_SET:
    if ((offset > DEV_MEM_SIZE) || (offset < 0)) {
      return -EINVAL;
    }
    filep->f_pos = offset;
    break;
  case SEEK_CUR:
    temp = filep->f_pos + offset;
    if ((temp > DEV_MEM_SIZE) || (temp < 0)) {
      return -EINVAL;
    }

    filep->f_pos = temp;
    break;
  case SEEK_END:
    temp = DEV_MEM_SIZE + offset;
    if ((temp > DEV_MEM_SIZE) || (temp < 0)) {
      return -EINVAL;
    }
    filep->f_pos = temp;

    break;
  default:
    return -EINVAL;
  }

  pr_info("updated file position %lld\n", filep->f_pos);
  return filep->f_pos;
}

ssize_t pcd_read(struct file *filep, char __user *buff, size_t count,
                 loff_t *f_pos) {
  pr_info("%zu byte(s) read requested\n", count);
  pr_info("current file position = %lld \n", *f_pos);
  /* Adjust the count */
  if ((*f_pos + count) > DEV_MEM_SIZE) {
    count = DEV_MEM_SIZE - *f_pos;
  }

  /*copy to user */
  if (copy_to_user(buff, &device_buffer[*f_pos], count)) {
    return -EFAULT;
  }

  /*update the current file position*/
  *f_pos += count;

  pr_info("number of bytes successfully read = %zu\n", count);
  pr_info("current file position = %lld \n", *f_pos);

  /*Return the number of bytes which have been successfully read*/
  return count;
}

ssize_t pcd_write(struct file *filep, const char __user *buff, size_t count,
                  loff_t *f_pos) {
  pr_info("%zu byte(s) write requested\n", count);

  pr_info("current file position = %lld \n", *f_pos);
  /* Adjust the count */
  if ((*f_pos + count) > DEV_MEM_SIZE) {
    count = DEV_MEM_SIZE - *f_pos;
  }

  if (!count) {
    pr_err("no space left on the device\n");
    return -ENOMEM;
  }

  /*copy from user */
  if (copy_from_user(&device_buffer[*f_pos], buff, count)) {
    return -EFAULT;
  }

  /*update the current file position*/
  *f_pos += count;

  pr_info("number of bytes successfully written = %zu\n", count);
  pr_info("current file position = %lld \n", *f_pos);

  /*Return the number of bytes which have been successfully written*/
  return count;
}

int pcd_open(struct inode *inode, struct file *filep) {
  pr_info(" open was successful\n");
  return 0;
}

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

struct class *class_pcd;
struct device *device_pcd;

static int __init pcd_driver_init(void) {

  int ret;
  /*Dynamically allocate a device number*/
  ret = alloc_chrdev_region(&device_number, 0, 1, "pcd_devices");
  if (ret < 0) {
    pr_err("could not allocate device number\n");
    goto out;
  }

  pr_info("Device number <major>:<minor> = %d:%d\n", MAJOR(device_number),
          MINOR(device_number));

  /*Initialize the cdev structure with fops*/
  cdev_init(&pcd_cdev, &pcd_fops);

  /*Register a device (cdev structure) with VFS*/
  pcd_cdev.owner = THIS_MODULE;
  ret = cdev_add(&pcd_cdev, device_number, 1);
  if (ret < 0) {
    pr_err("device couldn't created\n");
    goto unreg_chrdev;
  }

  /*create device class under /sys/calss
  side-note: after kernel version 6.3, class_create only takes single
  parameter, the name.*/
  /*class_pcd = class_create(THIS_MODULE, "pcd_class");*/
  class_pcd = class_create("pcd_class");
  if (IS_ERR(class_pcd)) {
    pr_err("Class creation failed\n");
    ret = PTR_ERR(class_pcd);
    goto cdev_del;
  }

  /*populate the sysfs with device information*/
  device_pcd = device_create(class_pcd, NULL, device_number, NULL, "pcd");
  if (IS_ERR(device_pcd)) {
    pr_err("Device creation failed\n");
    ret = PTR_ERR(device_pcd);
    goto class_del;
  }

  pr_info("Module init was successul\n");
  return 0;

class_del:
  class_destroy(class_pcd);
cdev_del:
  cdev_del(&pcd_cdev);
unreg_chrdev:
  unregister_chrdev_region(device_number, 1);
out:
  pr_err("module insertion failed\n");
  return ret;
}

static void __exit pcd_driver_cleanup(void) {
  device_destroy(class_pcd, device_number);
  class_destroy(class_pcd);
  cdev_del(&pcd_cdev);
  unregister_chrdev_region(device_number, 1);

  pr_info("module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yusuf Atalay");
MODULE_DESCRIPTION("A pseudo character device driver");
