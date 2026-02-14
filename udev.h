#ifndef UDEV_H
#define UDEV_H

#include "global.h"
#include "fops.h"

#define VENDOR_ID 0x256c
#define PRODUCT_ID 0x006e

static const struct usb_device_id id_list[] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ } // we need this here as a terminating entry for some reason
};

MODULE_DEVICE_TABLE(usb, id_list);

static int gaomon_probe(struct usb_interface *, const struct usb_device_id *);
static void gaomon_disconnect(struct usb_interface *);

static struct usb_device *gaomon_udev;
static struct usb_interface *gaomon_uinterface;

static struct usb_driver gaomon_udriver = {
    .name = "gaomon",
    .probe = gaomon_probe,
    .disconnect = gaomon_disconnect,
    .id_table = id_list
};
//we also want suspend and resume methods in this

static struct usb_class_driver gaomon_class = {
    .name = "gaomon%d",
    .fops = &fops
};

#endif
