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

#endif
