#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>

#include "../include/zarufs.h"
#include "zarufs_super.h"
#include "zarufs_utils.h"

static int zarufs_fill_super_block(struct super_block *sb,
                                   void *data,
                                   int silent);

struct dentry *
zarufs_mount_block_dev(struct file_system_type *fs_type,
                       int flags,
                       const char *dev_name,
                       void *data) {
  return mount_bdev(fs_type, flags, dev_name, data, zarufs_fill_super_block);
}

static int zarufs_fill_super_block(struct super_block *sb,
                                   void *data,
                                   int silent) {
  DBGPRINT("[ZARUFS] Hello, World.\n");
  return 0;
}

