#ifndef FOPS_H
#define FOPS_H

#include "global.h"

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

static int gaomon_open(struct inode *inode, struct file *filp){
	if(gaomon_device_open){
		return -EBUSY;
	}

	gaomon_device_open = 1;

	sprintf(msg, "If you can see this message, it worked!");
	msg_ptr = msg;
	try_module_get(THIS_MODULE);
	
	return 0;
}

static int gaomon_release(struct inode *inode, struct file *filp){
	gaomon_device_open = 0;

	module_put(THIS_MODULE);

	return 0;
}	

static ssize_t gaomon_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;

	if(msg_ptr == NULL){
		return -1;
	}

	if(*msg_ptr == 0){
		return 0;
	}

	while(length && *msg_ptr){
		put_user(*(msg_ptr++), buffer++);

		length--;
		bytes_read++;
	}

	printk(KERN_INFO "Message received: ", msg);

	return bytes_read;
}
		
static ssize_t gaomon_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
	return -EINVAL;
}

#endif
