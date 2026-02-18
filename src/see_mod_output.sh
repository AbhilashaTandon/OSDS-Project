sudo modprobe -r hid_uclogic
sudo modprobe -r usbhid
sudo insmod usb_driver.ko
cat /proc/devices
sudo rmmod usb_driver.ko
sudo dmesg | tail -10 | grep gaomon_driver
