#computer-science #programming #technology #linux #operating-systems
# Basics of Device Drivers

On Unix (and presumably also Linux), devices all interface with a specific file in `/dev` depending on the type of device. A sound card device driver, for example, would connect with `/dev/sound`, which can then be used by a user program without having to worry about the specifics of the hardware!

Devices have two numbers associated with them: a major number and a minor number. The major number refers to each specific device on the system, with the minor numbers used to distinguish between different pieces of hardware within the device. The kernel doesn't need to be aware of the minor number, it just uses the major number to identify which driver to use. 

There are two types of device drivers: block devices which use a buffer for requests and choose the best order to respond to them in. They also communicated in fixed-size "blocks". Most device drivers are character device drivers which don't need to bother with this stuff. 

# How to Device Drivering

To create a new char device named `coffee` with major/minor number 12 and 2, simply do `mknod /dev/coffee c 12 2`. You do not have to put your device files into `/dev`, but it is done by convention. Linus put his device files in `/dev`, and so should you. However, when creating a device file for testing purposes, it is probably OK to place it in your working directory where you compile the kernel module. Just be sure to put it in the right place when you’re done writing the device driver.

Each device is represented by a "file" structure. This isn't like a file in a file system, it's a metaphor for a virtual abstraction of a file that can be read from and written to. An instance of struct file is commonly named `filp`. (Phillip!)

There is also a "file_operations" struct which holds pointers to functions used for communicating with the device. An instance of it is usually referred to with the variable name `fops` (🦊!). 

Here are all the fops methods

```c
struct file_operations {
	struct module *owner;
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
	ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
	int (*iopoll)(struct kiocb *kiocb, bool spin);
	int (*iterate) (struct file *, struct dir_context *);
	int (*iterate_shared) (struct file *, struct dir_context *);
	__poll_t (*poll) (struct file *, struct poll_table_struct *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	unsigned long mmap_supported_flags;
	int (*open) (struct inode *, struct file *);
	int (*flush) (struct file *, fl_owner_t id);
	int (*release) (struct inode *, struct file *);
	int (*fsync) (struct file *, loff_t, loff_t, int datasync);
	int (*fasync) (int, struct file *, int);
	int (*lock) (struct file *, int, struct file_lock *);
	ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *
	, 
	 int);
	unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned
	
	 long, unsigned long, unsigned long);
	int (*check_flags)(int);
	int (*flock) (struct file *, int, struct file_lock *);
	ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *,
	
	 size_t, unsigned int);
	ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *,
	
	 size_t, unsigned int);
	int (*setlease)(struct file *, long, struct file_lock **, void **);
	long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
	void (*show_fdinfo)(struct seq_file *m, struct file *f);
	ssize_t (*copy_file_range)(struct file *, loff_t, struct file *,
	loff_t, size_t, unsigned int);
	loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in,
	struct file *file_out, loff_t pos_out,
	loff_t len, unsigned int remap_flags);
	int (*fadvise)(struct file *, loff_t, loff_t, int);
} __randomize_layout;
```

I won't need most of these thankfully. But the ones I do need I can supply and pass the pointers to those methods to functions in my initializer, and then the module can run them. 

Adding a device driver to the kernel is simple, just assign it a major number in the module's init function using this method, which will allocate a major device number for us.

```c
int alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count, const char *name);
```

Then we initialize a struct called `cdev` and use it with the following functions:
```c
void cdev_init(struct cdev *cdev, const struct file_operations *fops);
int cdev_add(struct cdev *p, dev_t dev, unsigned count);
```

Here is an example of using these functions in a module initialization:

```c
static int __init ioctl_init(void)
{
	dev_t dev;
	int ret;
	
	ret = alloc_chrdev_region(&dev, 0, num_of_dev, DRIVER_NAME);
	
	if(ret)
		return ret;
	
	test_ioctl_major = MAJOR(dev);
	cdev_init(&test_ioctl_cdev, &fops);
	ret = cdev_add(&test_ioctl_cdev, dev, num_of_dev);
	
	if (ret) {
		unregister_chrdev_region(dev, num_of_dev);
		return ret;
	}
	
	pr_alert("%s driver(major: %d) installed.\n", DRIVER_NAME, test_ioctl_major);
	return 0;
}
```

The macros `MAJOR()` and `MINOR()` can be used to access the current device drivers when supplied a `struct dev_t`.  We should also remember to unregister our device driver in our cleanup function using the `unregister_chrdev_region()` function. 

## Creating Device Files

Device files used to be created by kernels, but they're now created by userspace programs. The kernel has the job of placing the device class and device info into the `/sys` directory, after which the `udev` daemon (a userspace program) will create the device files. This is great for us since we don't have to do as much stuff. The following function creates a device class:

```c
struct class *cl = class_create(THIS_MODULE, "<device class name>");
```

which we can then supply with major and minor numbers like so (they are inside the `dev_t` struct we pass in). 

```c
device_create(cl, NULL, dev_t_instance, NULL, "<device name format>", …);
```

In our cleanup function we have to undo these in reverse order:

```c
device_destroy(cl, dev_t_instance);
class_destroy(cl);
```

## Reading and Writing

There are two functions we need to pass to fops to read and write from our device file, which must have the following signatures

```c
ssize_t (*read) (struct file *, char *, size_t, loff_t *);
ssize_t (*write) (struct file *, const char *, size_t, loff_t *);
```

The second arguments are buffers to read and write from/to. 

To avoid memory errors (buffer overflows/underflows and such), we can use some special functions.

```c
unsigned long __copy_to_user(void __user * to, const void * from, unsigned long n);
unsigned long __copy_from_user(void * to, const void __user * from, unsigned long n);
```

_to_: Destination address, in user space. 

_from_: Source address, in kernel space.

_n_: Number of bytes to copy

To safely read/write a single character, we use this function like so

```c
if(copy_to_user(buffer, &chr, 1) != 0){ return error_code; }
if (copy_from_user(&chr, buffer + len – 1, 1) != 0){ return error_code; }
```

