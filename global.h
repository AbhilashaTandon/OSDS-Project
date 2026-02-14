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
