make clean
make
sudo modprobe -r hid_uclogic
sudo modprobe -r usbhid
sudo rmmod gaomon_driver
sudo insmod gaomon_driver.ko
sudo rm /dev/gaomon_driver
sudo mknod /dev/gaomon_driver c 239 0
cat /dev/gaomon_driver > log.txt
sudo dmesg | grep gaomon_driver
