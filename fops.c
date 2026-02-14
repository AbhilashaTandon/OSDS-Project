#include "fops.h"

static int gaomon_open(struct inode *inode, struct file *filp){
    return 0;
}

static int gaomon_release(struct inode *inode, struct file *filp){
    return 0;
}	

static ssize_t gaomon_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
    return 0;
}

static ssize_t gaomon_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
    printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
    return -EINVAL;
}
