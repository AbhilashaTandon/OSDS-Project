savedcmd_usb_spoof.mod := printf '%s\n'   usb_spoof.o | awk '!x[$$0]++ { print("./"$$0) }' > usb_spoof.mod
