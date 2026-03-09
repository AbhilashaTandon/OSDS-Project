 Linux Input System

The Linux input system is the set of commands used to tell the OS about external inputs, such as from keyboards or mice. These functions are declared in the `linux/input.h` file, which needs to be included into any input driver. To write an input driver, the author must register their input device with the system and give information about what signals it will send. It can then report events such as key presses which will be recognized and interpreted by the system.

## Initialization

Within a driver init function, the first thing a driver should do for setting up an input device is check for the presence of the device itself. Then it may allocate a struct of type `struct input_dev` with the return value of the `input_allocate_device()` function. Then we need to set various bitfields on this struct to tell the system what events and input types it will transmit. 

```c
set_bit(<EVENT_TYPE>, input_dev->evbit);
set_bit(<BUTTON_TYPE>, input_dev->keybit);
```

Other useful bitmaps of `struct input_dev` are `relbit` and `absbit`, which are used for relative and absolute axes. Relative axes are used for mouse inputs while absolute are common for game controllers. For our purposes the relative axes are the only ones we need, which record the position of say a mouse relative to where it was last. 

There are too many button types to list, with some very weird ones probably only used in one device like `KEY_TEEN`, `KEY_DO_NOT_DISTURB`, or `KEY_RADAR_OVERLAY`. You can find all the key type declarations in `include/uapi/linux/input-event-codes.h`.


### Event Types

Thankfully there are few enough event types to list all of them here. 

```c
#define EV_SYN			0x00
#define EV_KEY			0x01
#define EV_REL			0x02
#define EV_ABS			0x03
#define EV_MSC			0x04
#define EV_SW			0x05
#define EV_LED			0x11
#define EV_SND			0x12
#define EV_REP			0x14
#define EV_FF			0x15
#define EV_PWR			0x16
#define EV_FF_STATUS		0x17
#define EV_MAX			0x1f
#define EV_CNT			(EV_MAX+1)
```

Of these `EV_KEY` is probably the most useful, used for keys and buttons. Reporting key events is done with this function:

```c
input_report_key(struct input_dev *dev, int code, int value)
```

`EV_REL` and `EV_ABS` are also quite useful for the aformentioned relative and absolute axes. Reporting relative axis inputs is thankfully pretty similar, using a function with identical arguments as the previous, but called `input_report_rel` instead.

`EV_REP` is used for key autorepetition.

### Metadata

But of course we need to define some more fields on our struct for metadata. The `name` attribute is self explanatory, while the `id` field is used for a struct that contains 16 bit ids for the bus location, and vendor, product, and version ids for the device.

## Functions

There are 2 function pointers attributes in an `input_dev` struct of use to us.

```c
int (*open)(struct input_dev *dev);
void (*close)(struct input_dev *dev);
```

These are where we can put our initialization and termination procedures, though we could also put them in our module `_init` and `_exit` functions. 

The most important function is our interrupt function, which should have the following signature:

```c
static irqreturn_t device_interrupt(int, void*);
```

The void pointer is just for us to pass some extra data to this function, which would probably just be the `struct input_dev`. This function is where we actually use our `input_report_*` functions. The return value is just one of these three constants:

```c
IRQ_NONE //interrupt not handled
IRQ_HANDLED //interrupt handled
IRQ_WAKE_THREAD //handler wants to wake the handler thread
```

I really only need to use the first two, and there's a helpful function `IRQ_RETVAL` which can turn a boolean representing whether the interrupt was successful into one of the first two of these values.
