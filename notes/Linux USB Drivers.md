# USBs on Linux

Much of the system for interacting with USBs is already handled by the hardware or the OS. When a new USB device is plugged into a computer, it is detected by the USB Host Controller or HC Driver, which is on the hardware level. The job of this device is to translate the physical information about the USB into something that conforms to the format of the USB protocol. Then the OS takes over with the generic USB core layer, which communicates the protocol information with device specific USB drivers, which themselves can then communicate with other system APIs to interact with the file system, network, terminal, etc.

The usb devices currently connected to a system can be viewed from the command line with the `lsusb -v` command. More details can be found with `cat /proc/bus/input/devices`, which uses the following format

```
Each line is tagged with a one-character ID for that line:

T = Topology (etc.)
B = Bandwidth (applies only to USB host controllers, which are
    virtualized as root hubs)
D = Device descriptor info.
P = Product ID info. (from Device descriptor, but they won't fit
    together on one line)
S = String descriptors.
C = Configuration descriptor info. (* = active configuration)
I = Interface descriptor info.
E = Endpoint descriptor info.
```

USB devices are structured in a hierarchical fashion. A USB ***device*** is assigned a device number on start and will have one or more USB ***configurations***.  For a given USB configuration, it may have multiple USB ***functions*** which provide a common functionality. Each configuration or function may have multiple USB ***interfaces***. For example, a printer may have separate interfaces for printing, scanning, faxing, etc. Typically the separate interfaces of a USB device each have their own driver, meaning one USB device can have multiple device drivers, but may also just use one. 

Each of these interfaces will have up to 16 ***endpoints***, which allow communication between the computer and the device. Endpoint 0 is the only bidirectional one and is required, the others each require a specific direction and type of data. Endpoints communicate over packages, and each endpoint has a maximum packet size. Drivers must often be aware of conventions such as flagging the end of bulk transfers using “short” (including zero length) packets. Endpoints are further divided into 4 types: 

- Control: request and send small packets used for configuration
- Bulk: request and send larger data packets
- Interrupt: similar to bulk but polled periodically
- Isochronous: send and receive data in real time, used for continuous streams such as for video and audio data

Control and Bulk use bandwidth as available and are synchronous while interrupt and isochronous are scheduled to provide guaranteed bandwidth. Asynchronous transfers are done with URBs ("USB Request Blocks"). 
# How to Write Your Driver

In order to write a driver we need to interface with the USB core, which can be done with the following functions.

```c
int usb_register(struct usb_driver *driver);
void usb_deregister(struct usb_driver *);
```

The `usb_driver` struct contains information about our driver as well as pointers to functions. Most important are `probe` and `disconnect`, which are used when adding or removing the USB device respectively. Other functions include `suspend`, `resume`, `pre_reset`, and `post_reset`. Optionally it can also be passed `fops` or `minor` attributes.

```c
static int probe(struct usb_interface *interface, const struct usb_device_id *id);
static void disconnect(struct usb_interface *interface);
```

The `probe` function is used to verify that the USB device is one the driver can actually use, often by checking vendor and product ids, and to set up data structures for the device such as buffers for each of the device's endpoints or URBs. (In this method I should list out all the device's endpoints and see if they are inputs or outputs). The function then returns a pointer to the driver's context structure. 

The `disconnect` function needs to free private data that has been allocated and shut down pending URBs. 

We also need to provide the `id_table`, which contains identification for the specific USB devices we want to connect with. Each USB device is identified with two 16 bit hex values, a **vendor id**, and a **product id**. We can simply pass in a `struct usb_device_id[]` array to pass these to our driver. 

```c
static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, id_table);
```

Annoyingly, we need the extra empty entry at the end. The `MODULE_DEVICE_TABLE` macro allows the driver to be loaded as soon as the device is plugged in. 

Once the device is plugged in and activated with `probe`, we can use the methods in our friend `fops` to actually talk to the device through the device file. As normal, the `open` function is called first. Here we need to increment our usage count and save some data to the file, which we can do with the `private_data` field, which is a helpful void pointer attribute of the file struct for storing any kind of data we need. 

Next we need to actually read from and write to the file. The simpler way to do this is to use the `usb_bulk_msg()` function, supplying it a our device, an endpoint, a buffer, the buffer's size, and a time limit value. If the time limit is exceeded it throws an error. We can then use the `copy_to_user()` function to copy the data from the buffer to user space. 

However this is not the best way for actually talking to the device. Instead we should set up proper URBs. But this method is probably fine for testing. 

# Meet Our Characters

We have a lot of structs to keep track of so It's probably helpful to give them all names. Also that's fun and cute.

## `struct usb_device` (AKA "Devon")
- The kernel's representation of a USB device
- Some useful attributes

```c
int devnum; // device number; address on a USB bus
enum usb_device_state state; // device state: configured, not attached, etc.
//Usbcore drivers should not set usbdev->state directly.  Instead use usb_set_device_state().
enum usb_device_speed speed; // device speed // high/full/low (or error)
struct usb_host_endpoint ep0; // endpoint 0 data (default control pipe)
struct device dev; // generic device interface
struct usb_device_descriptor descriptor; // USB device descriptor
unsigned int toggle[2]; // one bit for each endpoint, with ([0] = IN, [1] = OUT) endpoints
struct usb_host_config *config; // all of the device's configs
struct usb_host_config *actconfig; // the active configuration
struct usb_host_endpoint *ep_in[16]; // array of IN endpoints
struct usb_host_endpoint *ep_out[16]; // array of OUT endpoints
struct usb_device *parent; // usb hub unless root
```

- The following code snippet can be used to access all of the configs, interfaces, and endpoints of a device:

```c
for (i = 0; i < dev -> descriptor.bNumConfigurations; i++) {
  struct usb_config_descriptor * cfg = & dev -> config[i];
  
  for (j = 0; j < cfg -> bNumInterfaces; j++) {
    struct usb_interface * ifp = & cfg -> interface[j];
    
    for (k = 0; k < ifp -> num_altsetting; k++) {
      struct usb_interface_descriptor * as = & ifp -> altsetting[k];
      
      for (l = 0; l < as -> bNumEndpoints; l++) {
        struct usb_endpoint_descriptor * ep = & as -> endpoint[k];
      }
    }
  }
}
```

## `struct usb_driver` (AKA )
- identifies USB interface driver to usbcore
- Useful attributes:
```c
	const char *name; //self explanatory
    int (*probe) (struct usb_interface *intf, const struct usb_device_id *id); /* Called to see if the driver is willing to manage a particular interface on a device. If it is, probe returns zero and uses usb_set_intfdata() to associate driver-specific data with the interface. It may also use usb_set_interface() to specify the appropriate altsetting. If unwilling to manage the interface, return -ENODEV, if genuine IO errors occurred, an appropriate negative errno value. */
    void (*disconnect) (struct usb_interface *intf); //Called when the device is going to be suspended by the system either from system sleep or runtime suspend context. The return value will be ignored in system sleep context, so do NOT try to continue using the device if suspend fails in this case. Instead, let the resume or reset-resume routine recover from the failure.
    int (*suspend) (struct usb_interface *intf, pm_message_t message);
    int (*resume) (struct usb_interface *intf);
    const struct usb_device_id *id_table;
    struct device_driver driver;
    unsigned int supports_autosuspend:1;
```

## `struct file_operations` (AKA "fops")
## `struct usb_interface`
- what usb device drivers talk to


# URBs

A URB is a package of data sent to the device to execute some kind of transaction (i.e. to send or receive data). The way this works is strangely Javascript like. Submitting a URB request is an inherently asynchronous operation to which you have to prove a callback function ("completion handler"). 

Creating and freeing URBs are done with the following two functions.
```c
struct urb *usb_alloc_urb(int isoframes, int mem_flags);
void usb_free_urb(struct urb *urb);
```

`isoframes` should be set to 0 for interrupt endpoints. The `mem_flags` holds memory allocation flags `GFP_KERNEL` is one of them I think so I'll try using that. 

For our purposes, we need to use the `usb_fill_int_urb()` method to initialize the URB before submitting it.

```c
void usb_fill_int_urb(struct urb *urb, struct usb_device *dev, unsigned int pipe, void *transfer_buffer, int buffer_length, usb_complete_t complete_fn, void *context, int interval);
```

The transfer buffer needs to be read and written to with DMA, which means we need to allocate it with `kmalloc()`. It's risky to embed this buffer in a larger structure, so I may not want to do what `usb_skeleton.c` does.
`complete_fn` is our callback function. The `pipe` argument is very important here, this holds the information of whatever endpoint we're accessing. For my purposes I need the `usb_rcvintpipe()` macro to construct it. It holds the endpoint number, direction, type, and some other stuff. But we only need to supply it the `usb_device` struct and an endpoint address. 


To actually submit a URB, we call the following function
```c
int usb_submit_urb(struct urb *urb, int mem_flags);
```

Since the URB submissions are asynchronous, this immediately returns, giving back 0 or some error code. Use the following two functions to cancel a URB, the first one for asynchronous cancellations and the second for synchronous ones.
```c
int usb_unlink_urb(struct urb *urb);
void usb_kill_urb(struct urb *urb);
```

There's some concurrency troubles with these, I will just copy in something straight from the kernel documentation.

```
There is a lifetime issue to consider. An URB may complete at any time, and the completion handler may free the URB. If this happens while usb_unlink_urb() or usb_kill_urb() is running, it will cause a memory-access violation. The driver is responsible for avoiding this, which often means some sort of lock will be needed to prevent the URB from being deallocated while it is still in use.

On the other hand, since usb_unlink_urb may end up calling the completion handler, the handler must not take any lock that is held when usb_unlink_urb is invoked. The general solution to this problem is to increment the URB’s reference count while holding the lock, then drop the lock and call usb_unlink_urb or usb_kill_urb, and then decrement the URB’s reference count. You increment the reference count by calling :c:func`usb_get_urb`:

struct urb *usb_get_urb(struct urb *urb)

(ignore the return value; it is the same as the argument) and decrement the reference count by calling usb_free_urb(). Of course, none of this is necessary if there’s no danger of the URB being freed by the completion handler.
```

## Interrupt Transfers

Interrupt transfers happen periodically with intervals that are some unit of time times a power of 2. We can specify an interval in the `usb_fill_int_urb()` macro, and the submission function will round that down to the nearest valid interval time.

Unfortunately, URBs have to be manually restarted on completion, so we need to resubmit them in the completion handler. 
