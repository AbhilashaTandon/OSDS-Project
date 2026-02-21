#ifndef USB_DRIVER_H
#define USB_DRIVER_H

#include <asm/errno.h>
#include <asm/uaccess.h> // needed for put_user
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>	  // needed for macros
#include <linux/kernel.h> /* for sprintf() */
#include <linux/kref.h>	  //kernel ref counter
#include <linux/module.h> // needed by all modules
#include <linux/mutex.h>
#include <linux/printk.h> // needed for pr_info()
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h> /* for get_user and put_user */
#include <linux/usb.h>
#include <linux/version.h>
#define DRIVER_NAME "gaomon_driver"
#define BUF_LEN 80 /* Max length of the message from the device */

struct gaomon_data
{
	struct usb_device *udev;
	struct usb_interface uintf;
	unsigned char *buffer;
	size_t buffer_size = 4096;
	size_t buffer_usage = 0;
	static unsigned int device_no;
	static struct cdev cdev;
	static int major_no;
	static int minor_no;
};

// fops methods

static int gaomon_open(struct inode *, struct file *);
static int gaomon_release(struct inode *, struct file *);
static ssize_t gaomon_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t gaomon_write(struct file *, const char __user *, size_t, loff_t *);

// usb methods

static int gaomon_probe(struct usb_interface *intf, const struct usb_device_id *id);
static void gaomon_disconnect(struct usb_interface *intf);

static int __init gaomon_driver_init(void);
static void __exit gaomon_driver_exit(void);

#endif
