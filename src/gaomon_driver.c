// SPDX-License-Identifier: GPL-2.0
/*
 * Gaomon PD1560 USB tablet driver for Linux
 * based on the USB Skeleton driver v2.2 found in the Linux Kernel
 *
 * Copyright (C) 2026 Abhilasha Tandon <abhilashatandon167@gmail.com>
 * Copyright (C) 2001-2004 Greg Kroah-Hartman (greg@kroah.com)
 *
 */

#include "gaomon_driver.h"

// INPUT METHODS

/*
   void process_tablet_button_input(struct usb_gaomon *dev, int* i){ 
//we need to read in 4 bytes to process tablet button events

for(int j = 0; j < 4; j++){
assert(*i + j 
}

}

static irqreturn_t button_interrupt(int irq, void* dev){
unsigned char previous_byte = 0;
unsigned char current_byte = 0;
event_buffer = kzalloc(sizeof(char) * 12, GFP_KERNEL);
for(int i = 0; i < dev->input_filled; i++){
current_byte = *(dev->input_buffer + i);
if(current_byte != 0x08){
//all i/o event codes end in 0x08
previous_byte = current_byte;
continue;
}
//write 12 bytes to buffer

}
}
*/

// FOPS METHODS

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

        if(!try_module_get(THIS_MODULE)){
                return -ENODEV;
        }
        // pr_info("%s - Running open function.\n", DRIVER_NAME);
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
        module_put(THIS_MODULE);
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
        // pr_info("%s - Running urb irq callback function.\n", DRIVER_NAME);


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
                pr_info("%s - read urb of length %d.\n", DRIVER_NAME, dev->input_filled);
                //call input processing function 
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
        //	pr_info("%s - Running read io function.\n", DRIVER_NAME);

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

        if (dev->disconnected)
                return -ENODEV;

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
        if (!count)
                return 0;

        struct usb_gaomon *dev;
        int rv;
        bool ongoing_io;

        dev = file->private_data;

        if (dev->disconnected) {		/* disconnect() was called */
                pr_info("%s - Disconnect called, stopping read operation.\n", DRIVER_NAME);
                rv = -ENODEV;
                goto exit;
        }


        /* no concurrent readers */
        rv = mutex_lock_interruptible(&dev->io_mutex);
        if (rv < 0)
                return rv;


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
                // there may be a race condition here 
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
                else {
                        rv = chunk;
                }

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
        // pr_info( "%s - Probe function", DRIVER_NAME);
        struct usb_gaomon *dev;
        struct usb_endpoint_descriptor *endpoint;
        int retval;

        /* allocate memory for our device state and initialize it */
        dev = kzalloc(sizeof(*dev), GFP_KERNEL);
        if (!dev)
                return -ENOMEM;

        kref_init(&dev->kref);
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
        if(!dev){
                return;
        }

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

        if(!dev){
                return -EFAULT;
        }

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

        int error_code;
        if ((error_code = alloc_chrdev_region(&gaomon_device, USB_GAOMON_MINOR_BASE, 1,DRIVER_NAME)) < 0)	{
                pr_alert("%s - Error during allocating region. Error code %d.\n", DRIVER_NAME, error_code);
                return error_code;
        }

        gaomon_major_no = MAJOR(gaomon_device);
        gaomon_minor_no = MINOR(gaomon_device);

        pr_info("Major No: %d\n", gaomon_major_no);
        pr_info("Minor No: %d\n", gaomon_minor_no);


        cdev_init(&gaomon_char_device, &gaomon_fops);
        error_code = cdev_add(&gaomon_char_device, gaomon_device, 1);
        if(error_code){
                pr_alert("%s - Error registering char driver. Error code %d.\n", DRIVER_NAME, error_code);
                goto dealloc_region;
        }

        error_code = usb_register(&gaomon_driver);
        if(error_code){
                pr_alert("%s - Error during register. Error code %d.\n", DRIVER_NAME, error_code);
                goto del_cdev;
        }

        keyboard_input = input_allocate_device(); 
        if(!keyboard_input){
                pr_err("%s - Could not allocate input device for tablet buttons.\n", DRIVER_NAME);
                error_code = -ENOMEM;
                goto dereg_usb;
        }

        set_bit(EV_KEY, keyboard_input->evbit);
        set_bit(KEY_A, keyboard_input->evbit);
        set_bit(KEY_B, keyboard_input->evbit);
        set_bit(KEY_C, keyboard_input->evbit);
        set_bit(KEY_D, keyboard_input->evbit);
        set_bit(KEY_E, keyboard_input->evbit);
        set_bit(KEY_F, keyboard_input->evbit);
        set_bit(KEY_G, keyboard_input->evbit);
        set_bit(KEY_H, keyboard_input->evbit);
        set_bit(KEY_I, keyboard_input->evbit);
        set_bit(KEY_J, keyboard_input->evbit);

        error_code = input_register_device(keyboard_input);
        if(error_code){
                pr_alert("%s - Error registering input device. Error code %d.\n", DRIVER_NAME, error_code);
                goto deregister_input;
        }

deregister_input:
        input_unregister_device(keyboard_input);
dereg_usb:
        usb_deregister(&gaomon_driver);
del_cdev:
        cdev_del(&gaomon_char_device);
dealloc_region:
        unregister_chrdev_region(gaomon_device, 1);

        return error_code;
}


static void __exit gaomon_driver_exit(void){
        if(keyboard_input)
                input_unregister_device(keyboard_input);
        usb_deregister(&gaomon_driver);
        cdev_del(&gaomon_char_device);
        unregister_chrdev_region(gaomon_device, 1);
}

module_init(gaomon_driver_init);
module_exit(gaomon_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("USB driver for the Gaomon PD1560 graphics tablet");

