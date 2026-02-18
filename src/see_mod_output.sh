sudo modprobe -r hid_uclogic
sudo modprobe -r usbhid
sudo insmod gaomon_driver.ko
sudo dmesg | grep gaomon_driver
