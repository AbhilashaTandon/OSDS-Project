// SPDX-License-Identifier: GPL-2.0
/*
 * USB Gaomon driver
 *
 * Abhilasha Tandon
 *
 * This driver is based on the USB Skeleton driver provided in the Linux kernel source code.
 * but has been rewritten to be easier to read and use.
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
#include <linux/kernel.h> /* for sprintf() */
#include <linux/kref.h> //kernel ref counter #include <linux/module.h> // needed by all modules #include <linux/mutex.h>
#include <linux/module.h>
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

static void gaomon_delete(struct kref *kref)
{
	struct usb_gaomon *dev = to_gaomon_dev(kref);

	usb_free_urb(dev->input_urb);
	usb_put_intf(dev->interface);
	usb_put_dev(dev->udev);
	kfree(dev->input_buffer);
	kfree(dev);
}

static int gaomon_open(struct inode *inode, struct file *file)
{
	
	pr_info("%s - Running open function.\n", DRIVER_NAME);
	struct usb_gaomon *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);

	interface = usb_find_interface(&gaomon_driver, subminor);
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d\n",
				DRIVER_NAME, subminor);
		retval = -ENODEV;
		goto exit;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		pr_err ("%s - Can't get data from interface.\n",
				DRIVER_NAME );
		retval = -ENODEV;
		goto exit;
	}

	retval = usb_autopm_get_interface(interface);
	//used for autosuspend/resume functionality
	if (retval)
		goto exit;

	/* increment our usage count for the device */
	kref_get(&dev->kref);

	/* save our object in the file's private structure */
	file->private_data = dev;

exit:
	return retval;
}

static int gaomon_release(struct inode *inode, struct file *file)
{
	struct usb_gaomon *dev;

	dev = file->private_data;
	if (dev == NULL){
		pr_err ("%s - Global data stored in file is null pointer.\n", DRIVER_NAME);
		return -ENODEV;
	}

	/* allow the device to be autosuspended */
	usb_autopm_put_interface(dev->interface);

	/* decrement the count on our device */
	kref_put(&dev->kref, gaomon_delete);
	return 0;
}

static int gaomon_flush(struct file *file, fl_owner_t id)
{
	struct usb_gaomon *dev;
	int res;

	dev = file->private_data;
	if (dev == NULL){
		pr_err ("%s - Global data stored in file is null pointer.\n", DRIVER_NAME);
		return -ENODEV;
	}

	/* wait for io to stop */
	mutex_lock(&dev->io_mutex);
	gaomon_draw_down(dev);

	/* read out errors, leave subsequent opens a clean slate */
	spin_lock_irq(&dev->err_lock);
	res = dev->errors ? (dev->errors == -EPIPE ? -EPIPE : -EIO) : 0;
	dev->errors = 0;
	spin_unlock_irq(&dev->err_lock);

	mutex_unlock(&dev->io_mutex);

	return res;
}

static void gaomon_read_callback(struct urb *urb)
{
	pr_info("%s - Running urb irq callback function.\n", DRIVER_NAME);

	struct usb_gaomon *dev;
	unsigned long flags;

	dev = urb->context;

	spin_lock_irqsave(&dev->err_lock, flags);

	/* sync/async unlink faults aren't errors */
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
					urb->status == -ECONNRESET ||
					urb->status == -ESHUTDOWN))
			dev_err(&dev->interface->dev,
					"%s - nonzero write bulk status received: %d\n",
					__func__, urb->status);

		dev->errors = urb->status;
	}
	else{
		dev->input_filled = urb->actual_length;
	}
	dev->ongoing_read = 0;

	spin_unlock_irqrestore(&dev->err_lock, flags);

	wake_up_interruptible(&dev->input_wait);


	//resubmit urb because interrupt	
	// int rv = usb_submit_urb(urb, GFP_ATOMIC);
	// if(rv){
		// pr_err("%s - Error: failed resubmitting urb.\n", DRIVER_NAME);
	// }
	// else{
		// pr_info("%s - Successfully resubmitted urb.\n", DRIVER_NAME);
	// }
}

static int gaomon_do_read_io(struct usb_gaomon *dev, size_t count)

{
	pr_info("%s - Running read io function.\n", DRIVER_NAME);

	if(dev->input_urb->status == -EINPROGRESS){
		pr_info("%s - URB already pending, not resubmitting.\n", DRIVER_NAME);
		return 0;
	}

	int rv;

	unsigned int pipe = usb_rcvintpipe(dev->udev, dev->input_endpointAddr);

	rv = usb_pipe_type_check(dev->udev, pipe);
	if(rv)
		pr_err("%s - Error: pipe has invalid format. Error code %d.\n", DRIVER_NAME, rv);

	/* prepare a read */
	usb_fill_int_urb(dev->input_urb,
			dev->udev,
			pipe,
			dev->input_buffer,
			min(dev->input_size, count),
			gaomon_read_callback,
			dev, dev->input_interval);

	rv = usb_urb_ep_type_check(dev->input_urb);
	if(rv)
		pr_err("%s - Error: urb has invalid endpoint. Error code %d.\n", DRIVER_NAME, rv);

	/* tell everybody to leave the URB alone */
	spin_lock_irq(&dev->err_lock);
	dev->ongoing_read = 1;
	spin_unlock_irq(&dev->err_lock);

	/* submit int in urb, which means no data to deliver */
	dev->input_filled = 0;
	dev->input_copied = 0;

	/* do it */
	rv = usb_submit_urb(dev->input_urb, GFP_KERNEL);
	if (rv < 0) {
		dev_err(&dev->interface->dev,
				"%s - failed submitting read urb, error %d\n",
				__func__, rv);
		rv = (rv == -ENOMEM) ? rv : -EIO;
		spin_lock_irq(&dev->err_lock);
		dev->ongoing_read = 0;
		spin_unlock_irq(&dev->err_lock);
	}

	return rv;
}

static ssize_t gaomon_read(struct file *file, char *buffer, size_t count,
		loff_t *ppos)
{
	// pr_info("%s - Reading from device.\n", DRIVER_NAME);

	struct usb_gaomon *dev;
	int rv;
	bool ongoing_io;

	dev = file->private_data;

	if (!count)
		return 0;

	/* no concurrent readers */
	rv = mutex_lock_interruptible(&dev->io_mutex);
	if (rv < 0)
		return rv;

	if (dev->disconnected) {		/* disconnect() was called */
		pr_info("%s - Disconnect called, stopping read operation.\n", DRIVER_NAME);
		rv = -ENODEV;
		goto exit;
	}

	/* if IO is under way, we must not touch things */
retry:
	spin_lock_irq(&dev->err_lock);
	ongoing_io = dev->ongoing_read;
	spin_unlock_irq(&dev->err_lock);

	if (ongoing_io) {
		/* nonblocking IO shall not wait */
		if (file->f_flags & O_NONBLOCK) {
			rv = -EAGAIN;
			goto exit;
		}
		/*
		 * IO may take forever
		 * hence wait in an interruptible state
		 */
		rv = wait_event_interruptible(dev->input_wait, (!dev->ongoing_read));
		if (rv < 0)
			goto exit;
	}

	/* errors must be reported */
	rv = dev->errors;
	if (rv < 0) {
		/* any error is reported once */
		dev->errors = 0;
		/* to preserve notifications about reset */
		rv = (rv == -EPIPE) ? rv : -EIO;
		/* report it */
		goto exit;
	}

	/*
	 * if the buffer is filled we may satisfy the read
	 * else we need to start IO
	 */

	if (dev->input_filled) {
		/* we had read data */
		size_t available = dev->input_filled - dev->input_copied;
		size_t chunk = min(available, count);

		if (!available) {
			/*
			 * all data has been used
			 * actual IO needs to be done
			 */
			rv = gaomon_do_read_io(dev, count);
			if (rv < 0)
				goto exit;
			else
				goto retry;
		}
		/*
		 * data is available
		 * chunk tells us how much shall be copied
		 */

		if (copy_to_user(buffer,
					dev->input_buffer + dev->input_copied,
					chunk))
			rv = -EFAULT;
		else
			rv = chunk;

		// pr_info("%s - Copied %zd bytes to user space.\n", DRIVER_NAME, chunk);
		//need z because its a size_t

		dev->input_copied += chunk;

		/*
		 * if we are asked for more than we have,
		 * we start IO but don't wait
		 */
		if (available < count)
			gaomon_do_read_io(dev, count - chunk);
	} else {
		/* no data in the buffer */
		rv = gaomon_do_read_io(dev, count);
		if (rv < 0)
			goto exit;
		else
			goto retry;
	}
exit:
	mutex_unlock(&dev->io_mutex);
	return rv;
}


static ssize_t gaomon_write(struct file *file, const char *user_buffer,
		size_t count, loff_t *ppos)
{
	pr_alert("Sorry, this operation is not supported.\n");
	return -EINVAL;
}

static const struct file_operations gaomon_fops = {
	.owner =	THIS_MODULE,
	.read =		gaomon_read,
	.write =	gaomon_write,
	.open =		gaomon_open,
	.release =	gaomon_release,
	.flush =	gaomon_flush,
	.llseek =	noop_llseek,
};

/*
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver gaomon_class = {
	.name =		"gaomon%d",
	.fops =		&gaomon_fops,
	.minor_base =	USB_GAOMON_MINOR_BASE,
};

static int gaomon_probe(struct usb_interface *interface,
		const struct usb_device_id *id)
{
	pr_info( "%s - Probe function", DRIVER_NAME);
	struct usb_gaomon *dev;
	struct usb_endpoint_descriptor *endpoint;
	int retval;

	/* allocate memory for our device state and initialize it */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	kref_init(&dev->kref);
	sema_init(&dev->limit_sem, WRITES_IN_FLIGHT);
	mutex_init(&dev->io_mutex);
	spin_lock_init(&dev->err_lock);
	init_usb_anchor(&dev->submitted);
	init_waitqueue_head(&dev->input_wait);

	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = usb_get_intf(interface);

	/* set up the endpoint information */
	retval = usb_find_common_endpoints(interface->cur_altsetting,
			NULL, NULL, &endpoint, NULL);
	if (retval) {
		dev_err(&interface->dev,
				"Could not find endpoint\n");
		goto error;
	}

	dev->input_size = usb_endpoint_maxp(endpoint);
	dev->input_endpointAddr = endpoint->bEndpointAddress;
	dev->input_interval = endpoint->bInterval;
	dev->input_buffer = kmalloc(dev->input_size, GFP_KERNEL);
	if (!dev->input_buffer) {
		retval = -ENOMEM;
		goto error;
	}
	dev->input_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->input_urb) {
		retval = -ENOMEM;
		goto error;
	}


	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);

	/* we can register the device now, as it is ready */
	retval = usb_register_dev(interface, &gaomon_class);
	if (retval) {
		/* something prevented us from registering this driver */
		dev_err(&interface->dev,
				"Not able to get a minor for this device.\n");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	/* let the user know what node this device is now attached to */
	dev_info(&interface->dev,
			"USB Gaomon device now attached to USBGaomon-%d",
			interface->minor);
	return 0;

error:
	/* this frees allocated memory */
	kref_put(&dev->kref, gaomon_delete);

	return retval;
}

static void gaomon_disconnect(struct usb_interface *interface)
{
	struct usb_gaomon *dev;
	int minor = interface->minor;

	dev = usb_get_intfdata(interface);

	/* give back our minor */
	usb_deregister_dev(interface, &gaomon_class);

	/* prevent more I/O from starting */
	mutex_lock(&dev->io_mutex);
	dev->disconnected = 1;
	mutex_unlock(&dev->io_mutex);

	usb_kill_urb(dev->input_urb);
	usb_kill_anchored_urbs(&dev->submitted);

	/* decrement our usage count */
	kref_put(&dev->kref, gaomon_delete);

	dev_info(&interface->dev, "USB Gaomon #%d now disconnected", minor);
}

static void gaomon_draw_down(struct usb_gaomon *dev)
{
	int time;

	time = usb_wait_anchor_empty_timeout(&dev->submitted, 1000);
	if (!time)
		usb_kill_anchored_urbs(&dev->submitted);
	usb_kill_urb(dev->input_urb);
}

static int gaomon_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct usb_gaomon *dev = usb_get_intfdata(intf);

	if (!dev)
		return 0;
	gaomon_draw_down(dev);
	return 0;
}

static int gaomon_resume(struct usb_interface *intf)
{
	return 0;
}

static int gaomon_pre_reset(struct usb_interface *intf)
{
	struct usb_gaomon *dev = usb_get_intfdata(intf);

	mutex_lock(&dev->io_mutex);
	gaomon_draw_down(dev);

	return 0;
}

static int gaomon_post_reset(struct usb_interface *intf)
{
	struct usb_gaomon *dev = usb_get_intfdata(intf);

	/* we are sure no URBs are active - no locking needed */
	dev->errors = -EPIPE;
	mutex_unlock(&dev->io_mutex);

	return 0;
}

static struct usb_driver gaomon_driver = {
	.name =		"gaomon",
	.probe =	gaomon_probe,
	.disconnect =	gaomon_disconnect,
	.suspend =	gaomon_suspend,
	.resume =	gaomon_resume,
	.pre_reset =	gaomon_pre_reset,
	.post_reset =	gaomon_post_reset,
	.id_table =	gaomon_table,
	.supports_autosuspend = 1,
};
//INIT AND EXIT METHODS


static int __init gaomon_driver_init(void){
	pr_info( "%s - Hello!", DRIVER_NAME);

	int error_code;
	if ((error_code = alloc_chrdev_region(&gaomon_device, USB_GAOMON_MINOR_BASE, 1,DRIVER_NAME)) < 0)	{
		pr_alert("%s - Error during allocating region. Error code %d.\n", DRIVER_NAME, error_code);
		return error_code;
	}

	gaomon_major_no = MAJOR(gaomon_device);
	gaomon_minor_no = MINOR(gaomon_device);

	// pr_info("I'm %s. I'm a kernel module with major number %d and minor number %d.\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no);	
	 pr_info("To talk to\n");
	 pr_cont("the driver, create a dev file with\n");
	 pr_info("'mknod /dev/%s c %d %d'.\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no);
	// pr_info("Try various minor numbers. Try to cat and echo to\n");
	// pr_cont(" the device file.\n");
	// pr_info("Remove the device file and module when done.\n");

	cdev_init(&gaomon_char_device, &gaomon_fops);
	error_code = cdev_add(&gaomon_char_device, gaomon_device, 1);

	if(error_code){
		pr_alert("%s - Error registering char driver. Error code %d.\n", DRIVER_NAME, error_code);
		return error_code;
	}

	error_code = usb_register(&gaomon_driver);

	if(error_code){
		pr_alert("%s - Error during register. Error code %d.\n", DRIVER_NAME, error_code);
		return error_code;
	}

	pr_info("%s - USB driver registered successfully!.\n", DRIVER_NAME); 
	return 0;
}


static void __exit gaomon_driver_exit(void){
	pr_info("%s - Goodbye!", DRIVER_NAME);

	usb_deregister(&gaomon_driver);
	unregister_chrdev_region(gaomon_device, 1);
}

module_init(gaomon_driver_init);
module_exit(gaomon_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("USB driver for my Gaomon graphics tablet");

