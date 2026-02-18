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
#define BUF_LEN 80 /* Max length of the message from the device */

static unsigned int gaomon_device;
static struct cdev gaomon_char_device;

static int gaomon_open(struct inode *, struct file *);
static int gaomon_release(struct inode *, struct file *);
static ssize_t gaomon_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t gaomon_write(struct file *, const char __user *, size_t, loff_t *);

static int gaomon_probe(struct usb_interface *intf, const struct usb_device_id *id);
static void gaomon_disconnect(struct usb_interface *intf);
static ssize_t gaomon_read(struct file *file, char *buffer, size_t count, loff_t *ppos);

static int __init gaomon_driver_init(void);
static void __exit gaomon_driver_exit(void);

static struct class *gaomon_class;

static int gaomon_major_no; /* major number assigned to our device driver */
static int gaomon_minor_no; /* minor number assigned to our device driver */

enum {
	CDEV_NOT_USED,
	CDEV_EXCLUSIVE_OPEN,
};

/* Is device open? Used to prevent multiple access to device */
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

static char msg[BUF_LEN + 1]; /* The msg the device will give when asked */

#endif
