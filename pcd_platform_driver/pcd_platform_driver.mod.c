#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x122c3a7e, "_printk" },
	{ 0xf7be671b, "device_destroy" },
	{ 0x8f44466e, "cdev_del" },
	{ 0x37a0cba, "kfree" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x1399bb1, "class_create" },
	{ 0x633f674a, "__platform_driver_register" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x92ce99, "class_destroy" },
	{ 0x9bad3f4d, "platform_driver_unregister" },
	{ 0xb88db70c, "kmalloc_caches" },
	{ 0x4454730e, "kmalloc_trace" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0xa6f7a612, "cdev_init" },
	{ 0xf4407d6b, "cdev_add" },
	{ 0xd3044a78, "device_create" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x2fa5cadd, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "5F6719B46DBB9452FEB0ACE");
