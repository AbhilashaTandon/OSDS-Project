#ifndef GLOBAL_H
#define GLOBAL_H

#include <linux/module.h> // needed by all modules
#include <linux/printk.h> // needed for pr_info()
#include <linux/init.h> // needed for macros
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <asm/uaccess.h> // needed for put_user
#define DRIVER_NAME "gaomon_driver"
#define BUF_LEN 80

#define VENDOR_ID 0x256c
#define PRODUCT_ID 0x006e


static const struct usb_device_id id_list[] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ } // we need this here as a terminating entry for some reason
};

MODULE_DEVICE_TABLE(usb, id_list);

static int gaomon_open(struct inode *, struct file *);
static int gaomon_release(struct inode *, struct file *);
static ssize_t gaomon_read(struct file *, char *, size_t, loff_t *);
static ssize_t gaomon_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = gaomon_read,
	.write = gaomon_write,
	.open = gaomon_open,
	.release = gaomon_release
};

static int gaomon_probe(struct usb_interface *, const struct usb_device_id *);
static void gaomon_disconnect(struct usb_interface *);

static struct usb_device *gaomon_udev;
static struct usb_interface *gaomon_uinterface;

static struct usb_driver gaomon_udriver = {
    .name = "gaomon",
    .probe = gaomon_probe,
    .disconnect = gaomon_disconnect,
    .id_table = id_list
};
//we also want suspend and resume methods in this

static struct usb_class_driver gaomon_class = {
    .name = "gaomon%d",
    .fops = &fops
};
static dev_t gaomon_dev;
static struct cdev gaomon_cdev;
static int gaomon_major_no;
static int gaomon_minor_no;

static unsigned int gaomon_device_count = 0;
static struct urb *urb_input_buffer;
static unsigned char *input_buffer;
static size_t input_buffer_size;

static bool gaomon_device_open = false;
static char msg[BUF_LEN];
static char *msg_ptr;

#endif
