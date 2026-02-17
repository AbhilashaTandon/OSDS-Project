#ifndef USB_DRIVER_H
#define USB_DRIVER_H

#include <asm/uaccess.h> // needed for put_user
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h> // needed for macros
#include <linux/kernel.h>
#include <linux/kref.h> //kernel ref counter
#include <linux/module.h> // needed by all modules
#include <linux/mutex.h>
#include <linux/printk.h> // needed for pr_info()
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#define DRIVER_NAME "usb_driver"

static unsigned int my_device;
static struct cdev my_char_device;


static int my_usb_probe(struct usb_interface *intf, const struct usb_device_id *id);

static void my_usb_disconnect(struct usb_interface *intf);

static ssize_t my_usb_read(struct file *file, char *buffer, size_t count, loff_t *ppos);

static int __init usb_driver_init(void);

static void __exit usb_driver_exit(void);


#endif
