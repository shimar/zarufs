ZARUFS_SRC = src/zarufs.c

obj-m += zarufs.o
zarufs-objs := $(ZARUFS_SRC:.c=.o)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean


insmod:
	sudo insmod zarufs.ko

rmmod:
	sudo rmmod zarufs
