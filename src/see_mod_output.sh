make clean
make
sudo modprobe -r hid_uclogic
sudo modprobe -r usbhid
sudo rmmod gaomon_driver
sudo rm gaomon_driver
sudo insmod gaomon_driver.ko
sudo mknod gaomon_driver c 239 192
sudo dmesg | grep gaomon_driver
cat gaomon_driver
