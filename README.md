# Gaomon PD1560 Linux Driver

This is a Linux driver for the Gaomon PD1560 graphics tablet, created as a semester project for my graduate level operating systems class. 

I did all of my development within a virtual machine running a Linux Mint 22 guest OS, so I can't promise this will work on your machine without crashing your kernel or deleting your file system. Please use at your own risk.

## Installation

Clone the github repository and place it in the /drivers/hid/ folder of your kernel directory.

Then you can run the bash script to compile and add the driver. To remove it, run `sudo rmmod gaomon_driver` 

Run `sudo dmesg`. You should see output like so:

```
Major No: <some number>
Minor No: <some number>
```

Then run the following command to create the device file.

```
sudo mknod /dev/gaomon_driver c <major_no> <minor_no> 
```

# Usage

The driver should load automatically once you plug in your tablet, so no further configuration is necessary. 

To set keybindings/pen pressure settings/screen mapping, edit the `gaomon_driver.config` file in the root of the project directory. An example is given below.

## Contributing

Issues are welcome, please contact me if you would like to submit a pull request.

## License

[GPL v2]
