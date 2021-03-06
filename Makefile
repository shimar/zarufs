ZARUFS_SRC = src/zarufs.c \
						 src/zarufs_super.c \
	           src/zarufs_block.c \
	           src/zarufs_inode.c \
	           src/zarufs_dir.c \
	           src/zarufs_namei.c \
             src/zarufs_ialloc.c \
	           src/zarufs_file.c

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

mount:
	sudo mount -t zarufs -o loop ../zaru.img ../mnt

umount:
	sudo umount ../mnt

tags:
	etags include/*.h src/*.h src/*.c
