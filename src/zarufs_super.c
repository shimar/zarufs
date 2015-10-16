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
/* static void debug_print_zarufs_sb(struct zarufs_super_block* zsb); */


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
  struct zarufs_sb_info     *zsi;
  int                       block_size;
  int                       ret = -EINVAL;

  // allocate memory to zarufs_sb_info.
  zsi = kzalloc(sizeof(struct zarufs_sb_info), GFP_KERNEL);
  if (!zsi) {
    DBGPRINT("[ZARUFS] Error: unable to allocate mem for zarufs_sb_info.\n");
    ret = -ENOMEM;
    return ret;
  }

  // set device's block size and size bits to super block.
  block_size = sb_min_blocksize(sb, BLOCK_SIZE);
  DBGPRINT("[ZARUFS] Fill super block. block_size=%d\n", block_size);
  DBGPRINT("[ZARUFS] default block_size=%d\n", BLOCK_SIZE);

  if (!block_size) {
    DBGPRINT("[ZARUFS] Error: unable to set block_size.\n");
    goto error_read_sb;
  }
  
  if (!(bh = sb_bread(sb, 1))) {
    DBGPRINT("[ZARUFS] Error: failed to bread super block.\n");
    goto error_read_sb;
  }

  zsb = (struct zarufs_super_block*)(bh->b_data);
  sb->s_magic = le16_to_cpu(zsb->s_magic);
  if (sb->s_magic != ZARUFS_SUPER_MAGIC) {
    DBGPRINT("[ZARUFS] Error: magic of super block is %lu.\n", sb->s_magic);
    goto error_read_sb;
  }

  zsi->s_zsb = zsb;
  zsi->s_sbh = bh;

  // setup vfs super block.
  sb->s_fs_info = (void*) zsi;
  /* debug_print_zarufs_sb(zsb); */
  return 0;

 error_read_sb:
  kfree(zsi);
  return ret;
}


/* for debug. */
/* static void debug_print_zarufs_sb(struct zarufs_super_block* zsb) { */
/*   unsigned int value; */
/*   /\* int          i; *\/ */

/*   value = (unsigned int)(le32_to_cpu(zsb->s_inodes_count)); */
/*   DBGPRINT("[ZARUFS] s_inodes_count = %d\n", value); */
/*   return; */
/* } */
