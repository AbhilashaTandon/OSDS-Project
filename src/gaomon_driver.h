#ifndef GAOMON_DRIVER_H
#define GAOMON_DRIVER_H

#include <asm/errno.h>
#include <asm/uaccess.h> // needed for put_user
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h> // needed for macros
#include <linux/kernel.h> /* for sprintf() */
#include <linux/kref.h> //kernel ref counter
#include <linux/module.h> // needed by all modules
#include <linux/mutex.h>
#include <linux/printk.h> // needed for pr_info()
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h> /* for get_user and put_user */
#include <linux/usb.h>
#include <linux/version.h>
#define DRIVER_NAME "gaomon_driver"

//init/exit methods

static int __init gaomon_driver_init(void);
static void __exit gaomon_driver_exit(void);
static void cleanup(void);

//device ids

#define VENDOR_ID  0x256c
#define PRODUCT_ID 0x006e
#define MINOR_BASE 192

// gaomon: vendorid=256c productid=006e

static struct usb_device_id gaomon_id_table[] = {
	{USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{},
};
MODULE_DEVICE_TABLE(usb, gaomon_id_table);

//fops methods

int gaomon_open(struct inode *, struct file *);
int gaomon_release(struct inode *, struct file *);
ssize_t gaomon_read(struct file *, char __user *, size_t, loff_t *);
ssize_t gaomon_write(struct file *, const char __user *, size_t, loff_t *);

static struct file_operations gaomon_fops = {
	.read = gaomon_read,
	.write = gaomon_write,
	.open = gaomon_open,
	.release = gaomon_release,
	.owner = THIS_MODULE,
};

//usb driver methods

int gaomon_probe(struct usb_interface *intf, const struct usb_device_id *id);
void gaomon_disconnect(struct usb_interface *intf);

static struct usb_driver gaomon_driver = {
	.name = "%s",
	.id_table = gaomon_id_table,
	.probe = gaomon_probe,
	.disconnect = gaomon_disconnect,
};

static struct usb_class_driver gaomon_class_driver = {
	.name =		"gaomon%d",
	.fops =		&gaomon_fops,
	.minor_base =	MINOR_BASE,
};

//global struct

struct gaomon_data{
	struct usb_device *udev;
	struct usb_interface *uintf;

	//buffer stuff
	struct urb *urb_buffer;
	unsigned char *buffer;
	size_t buffer_size;
	size_t buffer_usage;
};

//device numbers
static unsigned int gaomon_device_no = 0;
static unsigned int gaomon_major_no = 0;
static unsigned int gaomon_minor_no = 0;

static struct usb_driver gaomon_driver;
static struct cdev gaomon_cdev;
static int cdev_added = 0;
static int driver_registered = 0;

#endif
