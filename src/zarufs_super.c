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

static void zarufs_put_super_block(struct super_block *sb);


static int zarufs_write_inode(struct inode* inode, struct writeback_control *wbc) {
  DBGPRINT("[ZARUFS] write_inode\n");
  return 0;
}

static void zarufs_destroy_inode(struct inode* inode) {
  DBGPRINT("[ZARUFS] destroy_inode\n");
  return;
}

/* static void zarufs_evict_inode(struct inode* inode) { */
/*   DBGPRINT("[ZARUFS] evict_inode\n"); */
/*   return; */
/* } */

static int zarufs_sync_fs(struct super_block *sb, int wait) {
  DBGPRINT("[ZARUFS] sync_fs\n");
  return 0;
}

static int zarufs_freeze_fs(struct super_block *sb) {
  DBGPRINT("[ZARUFS] freeze_fs\n");
  return 0;
}

static int zarufs_unfreeze_fs(struct super_block *sb) {
  DBGPRINT("[ZARUFS] unfreeze_fs\n");
  return 0;
}

static int zarufs_statfs(struct dentry *dentry, struct kstatfs *buf) {
  DBGPRINT("[ZARUFS] statfs\n");
  return 0;
}

static int zarufs_remount_fs(struct super_block* sb, int *len, char *buf) {
  DBGPRINT("[ZARUFS] remount_fs\n");
  return 0;
}

static int zarufs_show_options(struct seq_file *seq_file, struct dentry *dentry) {
  DBGPRINT("[ZARUFS] show_options\n");
  return 0;
}

static inline struct zarufs_sb_info *ZARUFS_SB(struct super_block *sb) {
  return ((struct zarufs_sb_info*) sb->s_fs_info);
}

static struct super_operations zarufs_super_ops = {
  .destroy_inode = zarufs_destroy_inode,
  .write_inode   = zarufs_write_inode,
  /* .evict_inode  = zarufs_evict_inode, */
  .put_super     = zarufs_put_super_block,
  .sync_fs       = zarufs_sync_fs,
  .freeze_fs     = zarufs_freeze_fs,
  .unfreeze_fs   = zarufs_unfreeze_fs,
  .statfs        = zarufs_statfs,
  .remount_fs    = zarufs_remount_fs,
  .show_options  = zarufs_show_options,
};



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
  struct inode              *root;
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
  DBGPRINT("[ZARUFS] device is: %s\n", sb->s_id);

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
  sb->s_op = &zarufs_super_ops;
  DBGPRINT("[ZARUFS] max file size=%lu\n", (unsigned long) sb->s_maxbytes);
  sb->s_fs_info = (void*) zsi;
  root = iget_locked(sb, ZARUFS_EXT2_ROOT_INO);
  if (IS_ERR(root)) {
    DBGPRINT("[ZARUFS] Error: failed to get root inode.\n");
    ret = PTR_ERR(root);
    goto error_mount;
  }

  unlock_new_inode(root);
  inc_nlink(root);
  root->i_mode = S_IFDIR;
  if (!S_ISDIR(root->i_mode)) {
    DBGPRINT("[ZARUFS] root is not directory.\n");
  }
  sb->s_root = d_make_root(root);
  if (!sb->s_root) {
    DBGPRINT("[ZARUFS] Error: failed to make root.\n");
    ret = -ENOMEM;
    goto error_mount;
  }
  le16_add_cpu(&zsb->s_mnt_count, 1);
  DBGPRINT("[ZARUFS] zarufs is mounted!\n");

  /* debug_print_zarufs_sb(zsb); */
  return 0;

 error_mount:
  brelse(bh);

 error_read_sb:
  kfree(zsi);
  return ret;
}


static void zarufs_put_super_block(struct super_block *sb) {
  struct zarufs_sb_info *zsi;
  zsi = ZARUFS_SB(sb);
  brelse(zsi->s_sbh);
  sb->s_fs_info = NULL;
  kfree(zsi);
}

/* for debug. */
/* static void debug_print_zarufs_sb(struct zarufs_super_block* zsb) { */
/*   unsigned int value; */
/*   /\* int          i; *\/ */

/*   value = (unsigned int)(le32_to_cpu(zsb->s_inodes_count)); */
/*   DBGPRINT("[ZARUFS] s_inodes_count = %d\n", value); */
/*   return; */
/* } */
