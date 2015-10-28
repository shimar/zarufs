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


static loff_t zarufs_max_file_size(struct super_block *sb) {
  int    file_blocks;
  int    nr_blocks;
  loff_t max_size;

  file_blocks = ZARUFS_NDIR_BLOCKS;
  nr_blocks = sb->s_blocksize / sizeof(u32);
  file_blocks += nr_blocks;
  file_blocks += nr_blocks * nr_blocks;
  file_blocks += nr_blocks * nr_blocks * nr_blocks;
  max_size = file_blocks * sb->s_blocksize;
  if (MAX_LFS_FILESIZE < max_size) {
    max_size = MAX_LFS_FILESIZE;
  }
  return max_size;
}

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
  unsigned long             sb_block = 1;

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
  
  /* read super block. */
  if (!(bh = sb_bread(sb, 1))) {
    DBGPRINT("[ZARUFS] Error: failed to bread super block.\n");
    goto error_read_sb;
  }

  zsb = (struct zarufs_super_block*)(bh->b_data);
  /* check magic number. */
  sb->s_magic = le16_to_cpu(zsb->s_magic);
  if (sb->s_magic != ZARUFS_SUPER_MAGIC) {
    DBGPRINT("[ZARUFS] Error: magic of super block is %lu.\n", sb->s_magic);
    goto error_read_sb;
  }

  /* check revision. */
  if (EXT2_GOOD_OLD_REV == le32_to_cpu(zsb->s_rev_level)) {
    DBGPRINT("[ZARUFS] Error: cannot mount old revision\n");
    goto error_mount;
  }

  /* setup the super block information. */
  zsi->s_zsb = zsb;
  zsi->s_sbh = bh;
  zsi->s_sb_block    = sb_block;
  zsi->s_mount_opt   = le32_to_cpu(zsb->s_default_mount_opts);
  zsi->s_mount_state = le16_to_cpu(zsb->s_state);

  if (zsi->s_mount_state != EXT2_VALID_FS) {
    DBGPRINT("[ZARUFS] Error: cannot mount invalid filesystems\n");
    goto error_mount;
  }

  if (le16_to_cpu(zsb->s_errors) == EXT2_ERRORS_CONTINUE) {
    DBGPRINT("[ZARUFS] Error: CONTNUE\n");
  } else if (le16_to_cpu(zsb->s_errors) == EXT2_ERRORS_PANIC) {
    DBGPRINT("[ZARUFS] Error: PANIC\n");
  } else {
    DBGPRINT("[ZARUFS] Error: READ ONLY\n");
  }

  if (le32_to_cpu(zsb->s_rev_level) != EXT2_DYNAMIC_REV) {
    DBGPRINT("[ZARUFS] Error: cannot mount unsupported revision\n");
    goto error_mount;
  }

  /* inode disc information cache. */
  zsi->s_inode_size = le16_to_cpu(zsb->s_inode_size);
  if (zsi->s_inode_size < 128 ||
      !is_power_of_2(zsi->s_inode_size) ||
      (block_size < zsi->s_inode_size)) {
    DBGPRINT("[ZARUFS] Error: cannot mount unsupported inode size %u\n",
             zsi->s_inode_size);
    goto error_mount;
  }

  zsi->s_first_ino = le32_to_cpu(zsb->s_first_ino);
  zsi->s_inodes_per_group = le32_to_cpu(zsb->s_inodes_per_group);
  zsi->s_inodes_per_block = sb->s_blocksize / zsi->s_inode_size;
  if (zsi->s_inodes_per_block == 0) {
    DBGPRINT("[ZARUFS] Error: bad inodes per block\n");
    goto error_mount;
  }

  zsi->s_itb_per_group = zsi->s_inodes_per_group / zsi->s_inodes_per_block;
  /* group dist information cache */
  zsi->s_blocks_per_group = le32_to_cpu(zsb->s_blocks_per_group);
  if (zsi->s_blocks_per_group == 0) {
    DBGPRINT("[ZARUFS] Error: bad blocks per block\n");
    goto error_mount;
  }

  zsi->s_groups_count = ((le32_to_cpu(zsb->s_blocks_count)
                          - le32_to_cpu(zsb->s_first_data_block) - 1)
                         / zsi->s_blocks_per_group) + 1;
  zsi->s_desc_per_block = sb->s_blocksize / sizeof(struct ext2_group_desc);
  zsi->s_gdb_count = (zsi->s_groups_count + zsi->s_desc_per_block - 1) / zsi->s_desc_per_block;

  /* fragment disc information cache. */
  zsi->s_frag_size = 1024 << le32_to_cpu(zsb->s_log_frag_size);
  if (zsi->s_frag_size == 0) {
    DBGPRINT("[ZARUFS] Error: bad fragment size\n");
    goto error_mount;
  }

  zsi->s_frags_per_block = sb->s_blocksize / zsi->s_frag_size;
  zsi->s_frags_per_group = le32_to_cpu(zsb->s_frags_per_group);

  /* default disc information cache. */
  zsi->s_resuid = make_kuid(&init_user_ns, le16_to_cpu(zsb->s_def_resuid));
  zsi->s_resgid = make_kgid(&init_user_ns, le16_to_cpu(zsb->s_def_resgid));
  
  // setup vfs super block.
  sb->s_op = &zarufs_super_ops;
  sb->s_maxbytes = zarufs_max_file_size(sb);
  sb->s_max_links = ZARUFS_LINK_MAX;

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
  int                   i;

  zsi = ZARUFS_SB(sb);

  /* release buffer cache for block group descripter. */
  for (i = 0; i < zsi->s_gdb_count; i++) {
    if (zsi->s_group_desc[i]) {
      brelse(zsi->s_group_desc[i]);
    }
  }
  kfree(zsi->s_group_desc);

  /* release buffer cache for super block. */
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
