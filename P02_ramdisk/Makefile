# If called directly from the command line, invoke the kernel build system.
ifeq ($(KERNELRELEASE),)

	KERNEL_SOURCE := /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) clean

# Otherwise KERNELRELEASE is defined; we've been invoked from the
# kernel build system and can use its language.
else

	obj-m += dor.o
	dor-y := ram_block.o ram_device.o partition.o

endif
