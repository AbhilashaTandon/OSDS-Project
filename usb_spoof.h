#ifndef USB_SPOOF_H
#define USB_SPOOF_H

#include <linux/module.h> // needed by all modules
#include <linux/printk.h> // needed for pr_info()
#include <linux/init.h> // needed for macros
#include <linux/cdev.h>
#include <linux/fs.h>
#define DRIVER_NAME "usb_spoofer"
#define BUF_LEN 80

static dev_t USB_dev;
static struct cdev USB_cdev;
static int USB_major_no;
static int USB_minor_no;

static bool USB_device_open = false;
static char msg[BUF_LEN];
static char *msg_ptr;

static int USB_open(struct inode *, struct file *);
static int USB_release(struct inode *, struct file *);
static ssize_t USB_read(struct file *, char *, size_t, loff_t *);
static ssize_t USB_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
	.read = USB_read,
	.write = USB_write,
	.open = USB_open,
	.release = USB_release
};

#endif
