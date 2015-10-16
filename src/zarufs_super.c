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
/* for debug. */
static void debug_print_zarufs_sb(struct zarufs_super_block* zsb);


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
  struct buffer_head        *bh;
  struct zarufs_super_block *zsb;
  int                       block_size;
  int                       ret = -EINVAL;
  //unsigned long             sb_block = 1;

  // set device's block size and size bits to super block.
  block_size = sb_min_blocksize(sb, BLOCK_SIZE);
  DBGPRINT("[ZARUFS] Fill super block. block_size=%d\n", block_size);
  DBGPRINT("[ZARUFS] default block_size=%d\n", BLOCK_SIZE);

  if (!block_size) {
    DBGPRINT("[ZARUFS] Error: unable to set block_size.\n");
    return ret;
  }
  
  if (!(bh = sb_bread(sb, 1))) {
    DBGPRINT("[ZARUFS] Error: failed to bread super block.\n");
    return ret;
  }

  zsb = (struct zarufs_super_block*)(bh->b_data);
  debug_print_zarufs_sb(zsb);
  return 0;
}



/* for debug. */
static void debug_print_zarufs_sb(struct zarufs_super_block* zsb) {
  unsigned int value;
  /* int          i; */

  value = (unsigned int)(le32_to_cpu(zsb->s_inodes_count));
  DBGPRINT("[ZARUFS] s_inodes_count = %d\n", value);
  return;
}
