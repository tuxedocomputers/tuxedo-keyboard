obj-m := ./src/tuxedo_keyboard.o

tuxedo_tuxedo-objs := ./src/tuxedo_keyboard.o

PWD := $(shell pwd)
KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

install:
	make -C $(KDIR) M=$(PWD) modules_install

dkmsadd:
	cp -R . /usr/src/tuxedo_keyboard-1
	dkms add -m tuxedo_keyboard -v 1

dkmsremove:
	dkms remove -m tuxedo_keyboard -v 1 --all
	rm -rf /usr/src/tuxedo_keyboard-1
