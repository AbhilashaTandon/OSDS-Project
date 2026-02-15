#computer-science #programming #technology #linux #operating-systems

"*Involvement in the development of Linux kernel modules requires a foundation in the C programming language and a track record of creating conventional programs intended for process execution. This pursuit delves into a domain where an unregulated pointer, if disregarded, may potentially trigger the total elimination of an entire filesystem, resulting in a scenario that necessitates a complete system reboot.*" — **Linux Kernel Module Programming Guide**

D:
## Commands

| Command                      | Usage                                                                                                              |
| ---------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| `lsmod`                      | **list currently loaded kernel modules**. this is long so I probably want to grep it to find what I'm looking for. |
| `sudo insmod module_name.ko` | load a kernel module                                                                                               |
| `sudo rmmod module_name.ko`  | unload a kernel module                                                                                             |
| `sudo dmesg`                 | show kernel module print debug messages                                                                            |

## Code Files

- Always include the following files:
```c
#include <linux/module.h> /* Needed by all modules */
#include <linux/printk.h> /* Needed for pr_info() */
```

A kernel module must have two functions: one that runs upon loading and one that runs when unloaded. These can be called whatever, though you need a special directive to mark them. 

```c
#include <linux/init.h> /* Needed for the macros */

static int __init hello_2_init(void);
static void __exit hello_2_exit(void);

module_init(hello_2_init);
module_exit(hello_2_exit);
```

Kernel modules also need to have directives that specify meta information for some reason.

```c
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author Nameson");
MODULE_DESCRIPTION("A device driver");
```

## CLI Arguments

To supply CLI arguments to your module, declare them as global variables and use the `module_param()` directive to declare them as arguments. Using global variables allows you to set default values.

```c
int myint = 3;
module_param(myint, int, 0);
```

This directive takes three arguments, the variable, its type, and some permissions thingy (for the corresponding file in sysfs).

Strings and arrays are handled with the `module_param_string()` and `module_param_array()` directives, which take a four arguments, the variable name, the type of each array entry, the number of elements in the array, and the permission argument. The argument for the number of elements can be set to NULL optionally. 

```c
int myintarray[2];
module_param_array(myintarray, int, NULL, 0); /* not interested in count */
short myshortarray[4];
int count;
module_param_array(myshortarray, short, &count, 0); /* put count into "count" variable */
```

To document CLI arguments, use the `MODULE_PARAM_DESC()` macro, which takes two arguments: the variable name and a string describing its usage. 
## Compiling Kernel Mods

Compiling kernel modules is done differently than regular programs. To do so, we need a makefile with the following structure:

```makefile
obj-m += kernelmod.o
kernelmod-objs := file-1.o file-2.o

PWD := $(CURDIR)

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
```

## Notes on Programming for Kernel Mods

- Print debugging can be accomplished with `pr_info()`, which works just like `printf()` I think.
- Since your module will be combined with the entire kernel, avoiding variable name collisions is important. To do this,  declare all your variables as static and to use a well-defined prefix for your symbols. By convention, all kernel prefixes are lowercase.
-  There is a C99 way of assigning to elements of a structure, called designated initializers. You should use this syntax in case someone wants to port your driver. It will help with compatibility: 
- ```c
  struct file_operations fops = {
				.read = device_read,
				.write = device_write,
				.open = device_open,
				.release = device_release
		};
  ```
