// SPDX-License-Identifier: GPL-2.0
/*
 * Gaomon PD1560 USB tablet driver for Linux
 * based on the USB Skeleton driver - 2.2 found in the Linux Kernel
 *
 * Copyright (C) 2026 Abhilasha Tandon <abhilashatandon167@gmail.com>
 * Copyright (C) 2001-2004 Greg Kroah-Hartman (greg@kroah.com)
 *
 */

#include "gaomon_driver.h"

static enum gaomon_tablet_buttons decode_button_code(int code){
        pr_info("%s - Processing button input %d.\n", DRIVER_NAME, code);
        switch(code){
                case 0x0001: return GAOMON_BUTTON_1;
                case 0x0002: return GAOMON_BUTTON_2;
                case 0x0004: return GAOMON_BUTTON_3;
                case 0x0008: return GAOMON_BUTTON_4;
                case 0x0010: return GAOMON_BUTTON_5;
                case 0x0020: return GAOMON_BUTTON_6;
                case 0x0040: return GAOMON_BUTTON_7;
                case 0x0080: return GAOMON_BUTTON_8;
                case 0x0100: return GAOMON_BUTTON_9;
                case 0x0200: return GAOMON_BUTTON_10;
                case 0x0000: return NONE;
                default: 
                             pr_alert("%s - Invalid button code %d.\n", DRIVER_NAME, code);
                             return NONE;
        }
}

static void handle_button_input(struct usb_gaomon *dev, unsigned char *buf_pos){

        /* 
         * if(*(buf_pos + 2) != 0x01){
         *         return false;
         * }
         * if(*(buf_pos + 3) != 0x01){
         *         return false;
         * }
         */

        int button_code = (*(buf_pos + 5) << 8) + *(buf_pos + 4);

        pr_info("%04x", button_code);

        enum gaomon_tablet_buttons button = decode_button_code(button_code);

        if(keyboard_input == NULL){
                pr_info("%s - Keyboard input device uninitialized.\n");
        }
        else if(button != NONE){
                input_report_key(keyboard_input, gaomon_key_bindings[button], 1);
        }
        else{
                input_report_key(keyboard_input, gaomon_key_bindings[dev->button_pressed], 0);
                input_sync(keyboard_input);
        }

        dev->button_pressed = button;

}

static void handle_pen_input(struct usb_gaomon *dev, unsigned char *buf_pos, bool pen_press){
        signed long long int x_coord = (*(buf_pos +  9) << 24) + (*(buf_pos +  8) << 16) + (*(buf_pos + 3) << 8) + *(buf_pos + 2); 
        signed long long int y_coord = (*(buf_pos + 11) << 24) + (*(buf_pos + 10) << 16) + (*(buf_pos + 5) << 8) + *(buf_pos + 4); 
        unsigned int pen_pressure = (*(buf_pos + 7) << 8) + *(buf_pos + 6);

        // report input here

        pr_info("%s - %lld\t%lld\t%d\n", DRIVER_NAME, x_coord, y_coord, pen_pressure);

        dev->mouse_x_coord = x_coord;
        dev->mouse_y_coord = y_coord;
}


static void gaomon_process_input(struct usb_gaomon *dev, int chunk){
        for(int i = 0; i < chunk-1; i++){
                unsigned char *buf_pos = dev->input_buffer + dev->input_copied + i;

                if(*buf_pos != 0x08){
                        continue;
                } 

                if(chunk - i < 12){
                        //not enough space for new input event
                        return;
                }

                switch(*(buf_pos + 1)){
                        case 0xe0:
                                handle_button_input(dev, buf_pos);
                                break;
                        case 0x80:
                                handle_pen_input(dev, buf_pos, false);
                                break;
                        case 0x81:
                                handle_pen_input(dev, buf_pos, true);
                                break;
                        default:
                                break;
                }

                i += 12;
        }
        /*
         * for(int j = 0; j < 12 && (i + j) < chunk; j++){
         *         pr_info("%d: %02x", j, *(buf_pos + j));
         * }
         * pr_info("\n");
         */
}

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
        }
        dev->ongoing_read = 0;

        spin_unlock_irqrestore(&dev->err_lock, flags);

        wake_up_interruptible(&dev->input_wait);

        gaomon_process_input(dev, dev->input_filled); 

        //resubmit urb because interrupt	
        /* int rv = usb_submit_urb(urb, GFP_ATOMIC);
         * if(rv){
         *         pr_err("%s - Error: failed resubmitting urb.\n", DRIVER_NAME);
         * }
         * else{
         *         pr_info("%s - Successfully resubmitted urb.\n", DRIVER_NAME);
         * }
         */
}

static int gaomon_do_read_io(struct usb_gaomon *dev, size_t count)

{
        // pr_info("%s - Running read io function.\n", DRIVER_NAME);

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
        pr_info("%s - Reading from device.\n", DRIVER_NAME);

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
        dev->button_pressed = NONE;
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

        keyboard_input = input_allocate_device();

        if(!keyboard_input){
                pr_err("%s - Could not allocate input device for tablet buttons.\n", DRIVER_NAME);
                return error_code;
        }

        keyboard_input->name = "Gaomon Tablet Buttons";  

        set_bit(EV_KEY, keyboard_input->evbit);
        set_bit(KEY_A, keyboard_input->keybit);
        set_bit(KEY_B, keyboard_input->keybit);
        set_bit(KEY_C, keyboard_input->keybit);
        set_bit(KEY_D, keyboard_input->keybit);
        set_bit(KEY_E, keyboard_input->keybit);
        set_bit(KEY_F, keyboard_input->keybit);
        set_bit(KEY_G, keyboard_input->keybit);
        set_bit(KEY_H, keyboard_input->keybit);
        set_bit(KEY_I, keyboard_input->keybit);
        set_bit(KEY_J, keyboard_input->keybit);

        error_code = input_register_device(keyboard_input);
        if(error_code){
                pr_alert("%s - Error registering input device. Error code %d.\n", DRIVER_NAME, error_code);
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
MODULE_DESCRIPTION("USB driver for the Gaomon PD1560 graphics tablet");

