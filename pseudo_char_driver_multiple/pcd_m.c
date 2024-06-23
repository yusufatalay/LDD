#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s :" fmt, __func__

#define NO_OF_DEVICES 4

#define MEM_SIZE_MAX_PCDEV1 1024
#define MEM_SIZE_MAX_PCDEV2 512
#define MEM_SIZE_MAX_PCDEV3 1024
#define MEM_SIZE_MAX_PCDEV4 512

/*pseudo device's memory*/
char device_buffer_pcdev1[MEM_SIZE_MAX_PCDEV1];
char device_buffer_pcdev2[MEM_SIZE_MAX_PCDEV2];
char device_buffer_pcdev3[MEM_SIZE_MAX_PCDEV3];
char device_buffer_pcdev4[MEM_SIZE_MAX_PCDEV4];

/*Device private data structure*/
struct pcdev_private_data {
  char *buffer;
  unsigned size;
  const char *serial_number;
  int perm;
  struct cdev cdev;
};

/*Driver private data structure*/
struct pcdrv_private_data {
  int total_devices;
  dev_t device_number;
  struct class *class_pcd;
  struct device *device_pcd;
  struct pcdev_private_data pcdev_data[NO_OF_DEVICES];
};

/*File access modes*/
#define RDONLY 0x01
#define WRONLY 0x10
#define RDWR 0x11

struct pcdrv_private_data pcdrv_data = {
    .total_devices = NO_OF_DEVICES,
    .pcdev_data = {[0] = {.buffer = device_buffer_pcdev1,
                          .size = MEM_SIZE_MAX_PCDEV1,
                          .serial_number = "PCDEV1",
                          .perm = RDONLY},
                   [1] = {.buffer = device_buffer_pcdev2,
                          .size = MEM_SIZE_MAX_PCDEV2,
                          .serial_number = "PCDEV2",
                          .perm = WRONLY},
                   [2] = {.buffer = device_buffer_pcdev3,
                          .size = MEM_SIZE_MAX_PCDEV3,
                          .serial_number = "PCDEV3",
                          .perm = RDWR},
                   [3] = {.buffer = device_buffer_pcdev4,
                          .size = MEM_SIZE_MAX_PCDEV4,
                          .serial_number = "PCDEV4",
                          .perm = RDWR}}};

loff_t pcd_llseek(struct file *filep, loff_t offset, int whence) {
  struct pcdev_private_data *pcdev_data =
      (struct pcdev_private_data *)filep->private_data;
  int max_size = pcdev_data->size;

  pr_info("lseek requested\n");

  loff_t temp = 0;
  pr_info("current file position %lld\n", filep->f_pos);

  switch (whence) {
  case SEEK_SET:
    if ((offset > max_size) || (offset < 0)) {
      return -EINVAL;
    }
    filep->f_pos = offset;
    break;
  case SEEK_CUR:
    temp = filep->f_pos + offset;
    if ((temp > max_size) || (temp < 0)) {
      return -EINVAL;
    }

    filep->f_pos = temp;
    break;
  case SEEK_END:
    temp = max_size + offset;
    if ((temp > max_size) || (temp < 0)) {
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
  struct pcdev_private_data *pcdev_data =
      (struct pcdev_private_data *)filep->private_data;
  int max_size = pcdev_data->size;

  pr_info("%zu byte(s) read requested\n", count);
  pr_info("current file position = %lld \n", *f_pos);

  /* Adjust the count */
  if ((*f_pos + count) > max_size) {
    count = max_size - *f_pos;
  }

  /*copy to user */
  if (copy_to_user(buff, pcdev_data->buffer + (*f_pos), count)) {
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
  struct pcdev_private_data *pcdev_data =
      (struct pcdev_private_data *)filep->private_data;
  int max_size = pcdev_data->size;

  pr_info("%zu byte(s) write requested\n", count);

  pr_info("current file position = %lld \n", *f_pos);
  /* Adjust the count */
  if ((*f_pos + count) > max_size) {
    count = max_size - *f_pos;
  }

  if (!count) {
    pr_err("no space left on the device\n");
    return -ENOMEM;
  }

  /*copy from user */
  if (copy_from_user(pcdev_data->buffer + (*f_pos), buff, count)) {
    return -EFAULT;
  }

  /*update the current file position*/
  *f_pos += count;

  pr_info("number of bytes successfully written = %zu\n", count);
  pr_info("current file position = %lld \n", *f_pos);

  /*Return the number of bytes which have been successfully written*/
  return count;
}

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

int pcd_open(struct inode *inode, struct file *filep) {
  int ret, minor_n;
  struct pcdev_private_data *pcdev_data;
  /*find out on which device file open was attempted by the user space*/
  minor_n = MINOR(inode->i_rdev);
  pr_info("minor access = %d\n", minor_n);

  /*get device's private data structure*/
  pcdev_data = container_of(inode->i_cdev, struct pcdev_private_data, cdev);
  /*to supply device private data to other methods of the driver*/
  filep->private_data = pcdev_data;

  /*check permission*/
  ret = check_permission(pcdev_data->perm, filep->f_mode);

  (!ret) ? pr_info("open was successfull\n")
         : pr_info("open was unsuccessful\n");

  return ret;
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

static int __init pcd_driver_init(void) {

  int ret, i;
  /*Dynamically allocate a device numbers*/
  ret = alloc_chrdev_region(&pcdrv_data.device_number, 0, NO_OF_DEVICES,
                            "pcd_devices");
  if (ret < 0) {
    pr_err("could not allocate device number\n");
    goto out;
  }

  /*create device class under /sys/class
  side-note: after kernel version 6.3, class_create only takes single
  parameter, the name.*/
  /*class_pcd = class_create(THIS_MODULE, "pcd_class");*/
  pcdrv_data.class_pcd = class_create("pcd_m_class");
  if (IS_ERR(pcdrv_data.class_pcd)) {
    pr_err("Class creation failed\n");
    ret = PTR_ERR(pcdrv_data.class_pcd);
    goto unreg_chrdev;
  }

  for (i = 0; i < NO_OF_DEVICES; i++) {
    pr_info("Device number <major>:<minor> = %d:%d\n",
            MAJOR(pcdrv_data.device_number + i),
            MINOR(pcdrv_data.device_number + i));

    /*Initialize the cdev structure with fops*/
    cdev_init(&pcdrv_data.pcdev_data[i].cdev, &pcd_fops);

    /*Register a device (cdev structure) with VFS*/
    pcdrv_data.pcdev_data[i].cdev.owner = THIS_MODULE;
    ret = cdev_add(&pcdrv_data.pcdev_data[i].cdev, pcdrv_data.device_number + i,
                   1);
    if (ret < 0) {
      pr_err("device couldn't created\n");
      goto cdev_del;
    }

    /*populate the sysfs with device information*/
    pcdrv_data.device_pcd =
        device_create(pcdrv_data.class_pcd, NULL, pcdrv_data.device_number + i,
                      NULL, "pcdev-%d", i + 1);
    if (IS_ERR(pcdrv_data.device_pcd)) {
      pr_err("Device creation failed\n");
      ret = PTR_ERR(pcdrv_data.device_pcd);
      goto class_del;
    }
  }

  pr_info("Module init was successul\n");

  return 0;

cdev_del:
class_del:
  for (; i >= 0; i--) {
    device_destroy(pcdrv_data.class_pcd, pcdrv_data.device_number + i);
    cdev_del(&pcdrv_data.pcdev_data[i].cdev);
  }
  class_destroy(pcdrv_data.class_pcd);
unreg_chrdev:
  unregister_chrdev_region(pcdrv_data.device_number, NO_OF_DEVICES);
out:
  pr_err("module insertion failed\n");
  return ret;
}

static void __exit pcd_driver_cleanup(void) {
  int i;
  for (i = 0; i > 0; i++) {
    device_destroy(pcdrv_data.class_pcd, pcdrv_data.device_number + i);
    cdev_del(&pcdrv_data.pcdev_data[i].cdev);
  }
  class_destroy(pcdrv_data.class_pcd);
  unregister_chrdev_region(pcdrv_data.device_number, NO_OF_DEVICES);
  pr_info("module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yusuf Atalay");
MODULE_DESCRIPTION("A pseudo character device driver which handles 4 devices");
