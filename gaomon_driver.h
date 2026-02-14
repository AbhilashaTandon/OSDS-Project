#ifndef GAOMON_DRIVER_H
#define GAOMON_DRIVER_H

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

static dev_t gaomon_dev;
static struct cdev gaomon_cdev;
static int gaomon_major_no;
static int gaomon_minor_no;

static bool gaomon_device_open = false;
static char msg[BUF_LEN];
static char *msg_ptr;

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



#endif
