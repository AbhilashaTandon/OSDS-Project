make clean
make
sudo modprobe -r hid_uclogic
sudo modprobe -r usbhid
sudo rmmod gaomon_driver
sudo insmod gaomon_driver.ko
cat /dev/gaomon_driver
sudo dmesg | grep gaomon_driver
