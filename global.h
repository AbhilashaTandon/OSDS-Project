#ifndef GLOBAL_H
#define GLOBAL_H

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

#define DRIVER_NAME "gaomon_driver"
#define BUF_LEN 80

#define VENDOR_ID 0x256c
#define PRODUCT_ID 0x006e

static const struct usb_device_id id_list[] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ } // we need this here as a terminating entry for some reason
};

MODULE_DEVICE_TABLE(usb, id_list);

struct gaomon_data{
	struct usb_device *udev;
	struct usb_interface *uintf;
	struct urb *input_urb; 
	unsigned char *input_buffer; 
	size_t input_buffer_size;
	size_t input_buffer_usage;
	size_t input_buffer_copied_to_user;
	__u8 in_endpoint_addr;
	__u8 out_endpoint_addr;
	bool is_reading;
	struct kref kref; //ref count
};


#define get_gaomon_data_from_ref(ref) container_of(ref, struct gaomon_data, kref)

//fops methods

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

//usb methods

static int gaomon_probe(struct usb_interface *, const struct usb_device_id *);
static void gaomon_disconnect(struct usb_interface *);
static int gaomon_device_id;

//other


static void gaomon_free(struct kref *kref);
//frees allocated memory if kref is 0


//structs

struct usb_driver gaomon_udriver = 
{
	.name = "gaomon",
    .probe = gaomon_probe,
    .disconnect = gaomon_disconnect,
    .id_table = id_list
};


static struct usb_class_driver gaomon_class = {
    .name = "gaomon%d",
    .fops = &fops
};

//vars

static int gaomon_major_no; 
static int gaomon_minor_no;
static struct cdev gaomon_cdev;


#endif
