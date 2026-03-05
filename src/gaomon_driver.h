// SPDX-License-Identifier: GPL-2.0
/*
 * Gaomon PD1560 USB tablet driver for Linux
 * based on the USB Skeleton driver - 2.2 found in the Linux Kernel
 *
 * Copyright (C) 2026 Abhilasha Tandon <abhilashatandon167@gmail.com>
 * Copyright (C) 2001-2004 Greg Kroah-Hartman (greg@kroah.com)
 *
 */

#include <asm/errno.h>
#include <asm/uaccess.h> // needed for put_user
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h> // needed for macros
#include <linux/kernel.h> 
#include <linux/kref.h>  
#include <linux/module.h> //needed by all modules
#include <linux/mutex.h>
#include <linux/printk.h> // needed for pr_info()
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h> /* for get_user and put_user */
#include <linux/usb.h>
#include <linux/version.h>

/* Define these values to match your devices */
#define USB_GAOMON_VENDOR_ID	0x256c
#define USB_GAOMON_PRODUCT_ID	0x006e
#define DRIVER_NAME "gaomon_driver"

/* table of devices that work with this driver */
static const struct usb_device_id gaomon_table[] = {
	{ USB_DEVICE(USB_GAOMON_VENDOR_ID, USB_GAOMON_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, gaomon_table);

static unsigned int gaomon_device;
static struct cdev gaomon_char_device;

static int gaomon_major_no; /* major number assigned to our device driver */
static int gaomon_minor_no; /* minor number assigned to our device driver */

/* Get a minor range for your devices from the usb maintainer */
#define USB_GAOMON_MINOR_BASE	0

/* our private defines. if this grows any larger, use your own .h file */
#define MAX_TRANSFER		(PAGE_SIZE - 512)
/*
 * MAX_TRANSFER is chosen so that the VM is not stressed by
 * allocations > PAGE_SIZE and the number of packets in a page
 * is an integer 512 is the largest possible packet on EHCI
 */
#define WRITES_IN_FLIGHT	8
/* arbitrarily chosen */

/* Structure to hold all of our device specific stuff */
struct usb_gaomon {
	struct usb_device	*udev;			/* the usb device for this device */
	struct usb_interface	*interface;		/* the interface for this device */
	struct semaphore	limit_sem;		/* limiting the number of writes in progress */
	struct usb_anchor	submitted;		/* in case we need to retract our submissions */
	struct urb		*input_urb;		/* the urb to read data with */
	unsigned char           *input_buffer;	/* the buffer to receive data */
	int 			input_interval;
	size_t			input_size;		/* the size of the receive buffer */
	size_t			input_filled;		/* number of bytes in the buffer */
	size_t			input_copied;		/* already copied to user space */
	__u8			input_endpointAddr;	/* the address of the bulk in endpoint */
	int			errors;			/* the last request tanked */
	bool			ongoing_read;		/* a read is going on */
	spinlock_t		err_lock;		/* lock for errors */
	struct kref		kref;
	struct mutex		io_mutex;		/* synchronize I/O with disconnect */
	unsigned long		disconnected:1;
	wait_queue_head_t	input_wait;		/* to wait for an ongoing read */
};
#define to_gaomon_dev(d) container_of(d, struct usb_gaomon, kref)

static struct usb_driver gaomon_driver;
static void gaomon_draw_down(struct usb_gaomon *dev);

static struct input_dev *keyboard_input;
