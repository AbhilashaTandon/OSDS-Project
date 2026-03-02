#ifndef USB_DRIVER_H
#define USB_DRIVER_H

#define min(x, y) ((x) < (y)) : (x) : (y)

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
#include <linux/kref.h> //kernel ref counter #include <linux/module.h> // needed by all modules #include <linux/mutex.h>
#include <linux/printk.h> // needed for pr_info()
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h> /* for get_user and put_user */
#include <linux/usb.h>
#include <linux/version.h>
#define DRIVER_NAME "gaomon_driver"
#define BUF_LEN 1024 /* Max length of the message from the device */

static unsigned int gaomon_device;
static struct cdev gaomon_char_device;

static int gaomon_open(struct inode *, struct file *);
static int gaomon_release(struct inode *, struct file *);
static ssize_t gaomon_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t gaomon_write(struct file *, const char __user *, size_t, loff_t *);

//static int gaomon_read_irq(struct gaomon_data *, size_t );
//prepares a urb and submits it
//static void gaomon_irq_callback(struct urb *);
//callback for urb creation
//possibly need to resubmit here

static int gaomon_probe(struct usb_interface *intf, const struct usb_device_id *id);
static void gaomon_disconnect(struct usb_interface *intf);

static int __init gaomon_driver_init(void);
static void __exit gaomon_driver_exit(void);

static int gaomon_major_no; /* major number assigned to our device driver */
static int gaomon_minor_no; /* minor number assigned to our device driver */

struct gaomon_data{
	struct usb_device *udev;
	struct usb_interface *uintf;
	struct urb *urb;
	unsigned char *buffer;
	size_t buffer_size;
	size_t buffer_usage; 
	struct usb_endpoint_descriptor *input_endpoint;
};

void cleanup(struct usb_interface *, const struct usb_device_id *, struct gaomon_data *);
//deallocates data if probe fails

#define VENDOR_ID  0x256c
#define PRODUCT_ID 0x006e
#define MINOR_BASE 0

// gaomon: vendorid=256c productid=006e

static struct usb_device_id gaomon_id_table[] = {
	{USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{},
};

static struct file_operations gaomon_fops = {
	.read = gaomon_read,
	.write = gaomon_write,
	.open = gaomon_open,
	.release = gaomon_release,
	.owner = THIS_MODULE,
};

static struct usb_class_driver gaomon_class_driver = {
	.name =		"gaomon%d",
	.fops =		&gaomon_fops,
	.minor_base =	MINOR_BASE,
};

MODULE_DEVICE_TABLE(usb, gaomon_id_table);


static struct usb_driver gaomon_driver = {
	.name = DRIVER_NAME,
	.id_table = gaomon_id_table,
	.probe = gaomon_probe,
	.disconnect = gaomon_disconnect,
};

#endif
