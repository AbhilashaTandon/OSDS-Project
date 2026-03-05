# Gaomon Tablet Reverse Engineering

The Gaomon tablet sends data to the computer over two USB endpoints. These are both contained in the single configuration and are the only two endpoints for the USB system. These are both labeled as human interface devices, with the boot interface subclass, and also both use the mouse protocol despite the fact that only one is used for mouse inputs. ¯\_(ツ)_/¯

These are also both interrupt input endpoints. All together this makes much simpler for us. 

Interface 0 is found at address 0x81 and has a max packet size of 64 bytes that it transmits every 2 milliseconds.
Interface 1 is found at address 0x82 and has a max packet size of 16 bytes that it transmits every 2 milliseconds.

Also take note that the values in the data stream are encoded as little endian, 0x8008 is transmitted as the byte 0x08 followed by the byte 0x80. Also unless otherwise specified, assume every value takes up 16 bits.

## Pen Inputs

There are 4 types of input messages we can receive from the pen. One tells us its screen position, another indicates a pen press, and there are two for each of the two buttons on the pen itself.

### Pen Position

When the 16 bit sequence 0x8008 is transmitted the tablet registered the pen was close enough to the screen to be detected. This is followed by the lower 2 bytes of the x coordinate (top of screen is 0), and then the y coordinate (left of screen is 0). Then comes the value 0, followed by the higher 2 bytes of the x coord, and then another 0.

```
0x8008 x_coord_lower y_coord 0x0000 x_coord_higher 0x0000
```

All of these chunks are 16 bits or 4 hex digits. I assume one or both of these 0s actually represent some value (presumably the second one is the higher 2 bytes of the y coordinate) but I haven't seen them be nonzero in my testing. My screen is wide enough that the higher 2 bytes of the x coordinate were used. The x coordinate values seemed to range from 0x00000 - 0x10b00, and the y coordinate values 0x0000 - 0x9000 very roughly.

### Pen Press

When the 16bit sequence 0x8108 is transmitted the tablet registered a pen press/click. The structure of this message is similar. 

```
0x8108 x_coord_lower y_coord pen_pressure x_coord_higher 0x0000
```

Again each of these chunks are 16 bits. The pen pressure value has a max value of 0x1fff, which is reassuring given that the tablet was advertised as having 8192 different levels of pressure sensitivity. So it's good I didn't get ripped off.

### Pen Buttons

The pen has two buttons on it, one closer to the pen nib which I will call the bottom button, and one farther which I will call the top button. A press of the bottom button is signalled by the 2 byte sequence 0x8208, and the top button by 0x8408. These messages are continuously set as long as the button is pressed. The following bytes have the same format as the pen position message, indicating the position of the pen above the screen.

```
0x8208 x_coord_lower y_coord 0x0000 x_coord_higher 0x0000
0x8408 x_coord_lower y_coord 0x0000 x_coord_higher 0x0000
```

Note that pen presses to the screen and button presses cannot be sent simultaneously. I believe the press to the screen overrides the button press.

# Tablet Inputs

Keyboard inputs have much the same format as mouse inputs. They are signalled by the 2 byte code 0xe008, which seems to be always followed by the code 0x0101. The full format is given below.

```
0xe008 0x0101 button 0x0000 0x0000 0x0000
```

Again all these values are 16 bits. Note these only apply to the 10 buttons on the tablet itself, and not the two buttons on the pen. The value of the button always has 1 bit set, and the position of this bit corresponds to a specific button.

```
Button 1: 0x0001
Button 2: 0x0002
Button 1: 0x0004
Button 1: 0x0008
Button 1: 0x0010
Button 1: 0x0020
Button 1: 0x0040
Button 1: 0x0080
Button 1: 0x0100
Button 1: 0x0200
```

A button value of 0x0000 indicates the button was released. Interestingly, despite the fact that each button is assigned its own bit doesn't seem to be possible to register multiple button presses at once. The tablet does not register a button press that occurs when another button is held.
