ZARUFS_SRC = src/zarufs.c \
						 src/zarufs_super.c \
	           src/zarufs_block.c \
	           src/zarufs_inode.c \
	           src/zarufs_dir.c \
	           src/zarufs_namei.c

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
