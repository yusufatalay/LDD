#include <linux/module.h>
#include <linux/platform_device.h>

#include "platform.h"

#undef pr_fmt
#define pr_fmt(fmt) "%s :" fmt, __func__

void pcdev_release(struct device *dev) { pr_info("Device relased\n"); }

/*Create 2 platform data*/
struct pcdev_platform_data pcdev_pdata[4] = {
    [0] = {.size = 512, .perm = RDWR, .serial_number = "PCDEV_SR_1"},
    [1] = {.size = 1024, .perm = RDWR, .serial_number = "PCDEV_SR_2"},
    [2] = {.size = 256, .perm = RDONLY, .serial_number = "PCDEV_SR_3"},
    [3] = {.size = 2048, .perm = WRONLY, .serial_number = "PCDEV_SR_4"},
};

/*Create 2 platform devices*/

struct platform_device platform_pcdev_1 = {
    .name = "pcdev-A1X",
    .id = 0,
    .dev = {.platform_data = &pcdev_pdata[0], .release = pcdev_release}};

struct platform_device platform_pcdev_2 = {
    .name = "pcdev-B1X",
    .id = 1,
    .dev = {.platform_data = &pcdev_pdata[1], .release = pcdev_release}};

struct platform_device platform_pcdev_3 = {
    .name = "pcdev-C1X",
    .id = 2,
    .dev = {.platform_data = &pcdev_pdata[2], .release = pcdev_release}};

struct platform_device platform_pcdev_4 = {
    .name = "pcdev-D1X",
    .id = 3,
    .dev = {.platform_data = &pcdev_pdata[3], .release = pcdev_release}};

struct platform_device *platform_pcdevs[] = {
    &platform_pcdev_1,
    &platform_pcdev_2,
    &platform_pcdev_3,
    &platform_pcdev_4,
};

static int __init pcdev_platform_init(void) {
  /*Register platform device*/
  platform_add_devices(platform_pcdevs, ARRAY_SIZE(platform_pcdevs));
  pr_info("Device setup module loaded\n");
  return 0;
}

static void __exit pcdev_platform_exit(void) {
  platform_device_unregister(&platform_pcdev_1);
  platform_device_unregister(&platform_pcdev_2);
  platform_device_unregister(&platform_pcdev_3);
  platform_device_unregister(&platform_pcdev_4);
  pr_info("Device setup module unloaded\n");
}

module_init(pcdev_platform_init);
module_exit(pcdev_platform_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Module which registers platform devices ");
