#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "../include/zarufs.h"
#include "zarufs_utils.h"
#include "zarufs_inode.h"
#include "zarufs_block.h"
#include "zarufs_ialloc.h"

static long
find_directory_group(struct super_block *sb, struct inode *parent);

struct buffer_head*
read_inode_bitmap(struct super_block *sb, unsigned long block_group);

struct inode*
zarufs_alloc_new_inode(struct inode *dir, umode_t mode, const struct qstr *qstr) {
  struct super_block       *sb;
  struct buffer_head       *bitmap_bh;
  struct buffer_head       *bh_gdesc;

  struct inode             *inode;    /* new inode */
  ino_t                    ino;
  struct ext2_group_desc   *gdesc;
  struct zarufs_super_block *zsb;
  struct zarufs_inode_info *zi;
  struct zarufs_sb_info    *zsi;

  unsigned long            group;
  int                      i;
  int                      err;

  /* allocate vfs new inode. */
  sb = dir->i_sb;
  if (!(inode = new_inode(sb))) {
    return(ERR_PTR(-ENOMEM));
  }

  bitmap_bh = NULL;
  ino       = 0;
  zsi       = ZARUFS_SB(sb);
  if (S_ISDIR(mode)) {
    group = find_directory_group(sb, dir);
  } else {
    /* as for now allocating inode for file is not support. */
    err = -ENOSPC;
    goto fail;
  }

  if (group == -1) {
    err = -ENOSPC;
    goto fail;
  }

  for (i = 0; i < zsi->s_groups_count; i++) {
    brelse(bitmap_bh);
    if (!(bitmap_bh = read_inode_bitmap(sb, group))) {
      err = -EIO;
      goto fail;
    }
    ino = 0;
    /* find free inode. */
  repeat_in_this_group:
    ino = find_next_zero_bit_le((unsigned long*) bitmap_bh->b_data,
                                zsi->s_inodes_per_group,
                                ino);
    if (ZARUFS_SB(sb)->s_inodes_per_group <= ino) {
      /* cannot find ino. bitmap is already full. */
      group++;
      if (group <= zsi->s_groups_count) {
        group = 0;
      }
      continue;
    }

    /* allocate inode atomically. */
    if (ext2_set_bit_atomic(get_sb_blockgroup_lock(zsi, group),
                            (int) ino,
                            bitmap_bh->b_data)) {
      /* already set the bitmap. */
      ino++;
      if (zsi->s_inodes_per_group <= ino) {
        group++;
        if (zsi->s_groups_count <= group) {
          group = 0;
        }
        continue;
      }
      /* try to find in the same group. */
      goto repeat_in_this_group;
    }
    goto got;
  }
  /* cannot find free inode. */
  err = -ENOSPC;
  goto fail;

  /* found free inode. */
 got:
  zi  = ZARUFS_I(inode);
  zsb = zsi->s_zsb;
  mark_buffer_dirty(bitmap_bh);
  if (sb->s_flags & MS_SYNCHRONOUS) {
    sync_dirty_buffer(bitmap_bh);
  }
  brelse(bitmap_bh);

  /* get absolute inode number. */
  ino += (group * ZARUFS_SB(sb)->s_inodes_per_group) + 1;
  if ((ino < zsi->s_first_ino) ||
      (le32_to_cpu(zsb->s_inodes_count) < ino)) {
    ZARUFS_ERROR("[ZARUFS] %s: insane inode number. ino=%lu, group=%lu\n",
                 __func__, (unsigned long) ino, group);
    err = -EIO;
    goto fail;
  }

  /* update group descriptor. */
  gdesc    = zarufs_get_group_descriptor(sb, group);
  bh_gdesc = zarufs_get_gdesc_buffer_cache(sb, group);
  percpu_counter_add(&zsi->s_freeinodes_counter, -1);
  if (S_ISDIR(mode)) {
    percpu_counter_inc(&zsi->s_dirs_counter);
  }

  spin_lock(get_sb_blockgroup_lock(zsi, group));
  {
    le16_add_cpu(&gdesc->bg_free_inodes_count, -1);
    if (S_ISDIR(mode)) {
      le16_add_cpu(&gdesc->bg_used_dirs_count, 1);
    }
  }
  spin_unlock(get_sb_blockgroup_lock(zsi, group));
  mark_buffer_dirty(bh_gdesc);

  /* initialize vfs inode. */
  inode_init_owner(inode, dir, mode);
  inode->i_ino = ino;
  inode->i_blocks = 0;
  inode->i_mtime = CURRENT_TIME_SEC;
  inode->i_atime = inode->i_mtime;
  inode->i_ctime = inode->i_mtime;

  /* initialize zarufs inode information. */
  memset(zi->i_data, 0, sizeof(zi->i_data));
  zi->i_flags = ZARUFS_I(dir)->i_flags & EXT2_FL_INHERITED;
  if (S_ISDIR(mode)) {
    /* nop */
  } else if (S_ISREG(mode)) {
    zi->i_flags &= EXT2_REG_FLMASK;
  } else {
    zi->i_flags &= EXT2_OTHER_FLMASK;
  }

  zi->i_faddr     = 0;
  zi->i_frag_no   = 0;
  zi->i_frag_size = 0;
  zi->i_file_acl  = 0;
  zi->i_dir_acl   = 0;
  zi->i_dtime     = 0;
  /* zi->i_block_allock_info = NULL; */
  zi->i_state     = EXT2_STATE_NEW;

  zarufs_set_vfs_inode_flags(inode);
  /* insert vfs inode to hash table. */
  if (insert_inode_locked(inode) < 0) {
    ZARUFS_ERROR("[ZARUFS] %s: inode number already in use[%lu]\n",
                 __func__, (unsigned long) ino);
    err = -EIO;
    goto fail;
  }
  mark_inode_dirty(inode);
  DBGPRINT("[ZARUFS] allocating new inode %lu\n",
           (unsigned long) inode->i_ino);
  return (inode);

  /* allocation of new inode is failed. */
 fail:
  make_bad_inode(inode);
  iput(inode);
  return(ERR_PTR(err));
}

unsigned long
zarufs_count_free_inodes(struct super_block *sb) {
  unsigned long freei;
  int           group;

  freei = 0;
  for (group = 0; group < ZARUFS_SB(sb)->s_groups_count; group++) {
    struct ext2_group_desc *cur_desc;
    cur_desc = zarufs_get_group_descriptor(sb, group);
    if (!cur_desc) {
      continue;
    }
    freei += le16_to_cpu(cur_desc->bg_free_inodes_count);
  }
  return (freei);
}

unsigned long
zarufs_count_directories(struct super_block *sb) {
  unsigned long dir_count;
  int           i;

  dir_count = 0;
  for (i = 0; i < ZARUFS_SB(sb)->s_groups_count; i++) {
    struct ext2_group_desc *gdesc;
    if (!(gdesc = zarufs_get_group_descriptor(sb, i))) {
      continue;
    }
    dir_count += le16_to_cpu(gdesc->bg_used_dirs_count);
  }
  return(dir_count);
}


static long
find_directory_group(struct super_block *sb, struct inode *parent) {
  struct ext2_group_desc *best_desc;
  unsigned long          groups_count;
  unsigned long          avefreei;
  int                    group;
  int                    best_group;

  best_desc    = NULL;
  groups_count = ZARUFS_SB(sb)->s_groups_count;
  avefreei     = zarufs_count_free_inodes(sb) / groups_count;
  best_group   = -1;

  for (group = 0; group < groups_count; group++) {
    struct ext2_group_desc *cur_desc;
    cur_desc = zarufs_get_group_descriptor(sb, group);
    if (!cur_desc || !cur_desc->bg_free_inodes_count) {
      /* if there is no avaible inode or invalid descriptor, go next  */
      continue;
    }
    if (le16_to_cpu(cur_desc->bg_free_inodes_count) < avefreei) {
      /* if number of inodes is less than average, go next */
      continue;
    }
    if (!best_desc ||
        (le16_to_cpu(best_desc->bg_free_blocks_count) <
         le16_to_cpu(cur_desc->bg_free_blocks_count))) {
      /* found canditate */
      best_group = group;
      best_desc  = cur_desc;
    }
  }
  return(best_group);
}

struct buffer_head*
read_inode_bitmap(struct super_block *sb, unsigned long block_group) {
  struct ext2_group_desc *gdesc;
  struct buffer_head     *bh;

  if (!(gdesc = zarufs_get_group_descriptor(sb, block_group))) {
    return(NULL);
  }

  if (!(bh = sb_bread(sb, le32_to_cpu(gdesc->bg_inode_bitmap)))) {
    ZARUFS_ERROR("[ZARUFS] %s: cannot read inode bitmap.\n", __func__);
    ZARUFS_ERROR("[ZARUFS] block_group=%lu, inode_bitmap=%u\n",
                 block_group, le32_to_cpu(gdesc->bg_inode_bitmap));
  }
  return(bh);
}
