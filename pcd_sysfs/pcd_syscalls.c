#include "pcd_platform_driver_device_tree_sysfs.h"

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
