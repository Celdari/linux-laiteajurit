
# To build modules outside of the kernel tree, we run "make"
# in the kernel source tree; the Makefile these then includes this
# Makefile once again.
# This conditional selects whether we are being included from the
# kernel Makefile or not.
ifeq ($(KERNELRELEASE),)

    # Assume the source tree is where the running kernel was built
    # You should set KERNELDIR in the environment if it's elsewhere
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    # The current directory is passed to sub-makes as argument
    PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) --debug=b M=$(PWD) modules

modules_install: modules
	#insmod hello.ko 
	#insmod devdemo.ko	
	#insmod kdataalign.ko
	#insmod blockdemo.ko
	insmod devfsdemo.ko 
	#(MAKE) -C $(KERNELDIR) --debug=b M=$(PWD) modules_install

.REINSTALL: .UNINSTALL modules_install

.UNINSTALL: 
	#rmmod hello.ko
	#rmmod devdemo.ko
	#rmmod blockdemo.ko
	rmmod devfsdemo.ko || echo "Nothing to remove\n"

clean:
	#rmmod hello.ko 
	#rmmod devdemo.ko
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

.PHONY: modules modules_install clean

else
    #Specific file build
    obj-m := devfsdemo.o

    # called from kernel build system: just declare what our modules are
    #obj-m := hello.o hellop.o devdemo.o seq.o jit.o jiq.o sleepy.o complete.o \
             silly.o faulty.o kdatasize.o kdataalign.o
endif


