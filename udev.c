#include "udev.h"


static int gaomon_probe(struct usb_interface *interface, const struct usb_device_id *device_id){
    int error_code;
    
    gaomon_udev = usb_get_dev(interface_to_usbdev(interface);
    gaomon_interface = usb_get_intf(interface);

    error_code = usb_register_dev(gaomon_interface, &gaomon_class);

    if(error_code) {
        printk(KERN_ALERT "Unable to register this driver. Not able to get a minor for this device. Error code %d\n", error_code);
        return error_code;
    }
    
    return error_code;
    
}  
static void gaomon_disconnect(struct usb_interface *interface){
    usb_deregister_dev(gaomon_interface, &gaomon_class);
}
