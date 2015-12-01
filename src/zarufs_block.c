/* zarufs_block.c */
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "../include/zarufs.h"
#include "zarufs_block.h"
#include "zarufs_utils.h"

#define IN_RANGE(b, first, len) (((first) <= (b)) \
                                 && ((b) <= (first) + (len) + 1))

static int
is_group_sparse(int group);

static int
test_root(int group, int multiple);

static int
has_free_blocks(struct zarufs_sb_info *zsi);

static struct buffer_head*
read_block_bitmap(struct super_block *sb, unsigned long block_group);

static int
valid_block_bitmap(struct super_block *sb,
                   struct ext2_group_desc *gdesc,
                   unsigned long block_group,
                   struct buffer_head *bh);

struct ext2_reserve_window;
static int
try_to_allocate(struct super_block *sb,
                unsigned long group,
                struct buffer_head *bitmap_bh,
                unsigned long grp_goal,
                unsigned long *count,
                struct ext2_reserve_window *my_rsv);

static void
adjust_group_blocks(struct super_block *sb,
                    unsigned long group_no,
                    struct ext2_group_desc *gdesc,
                    struct buffer_head *bh,
                    long count);

struct ext2_group_desc*
zarufs_get_group_descriptor(struct super_block *sb,
                            unsigned int block_group) {
  struct zarufs_sb_info  *zsi;
  unsigned long          gdesc_index;
  unsigned long          gdesc_offset;
  struct ext2_group_desc *group_desc;

  zsi = ZARUFS_SB(sb);

  if (zsi->s_groups_count <= block_group) {
    ZARUFS_ERROR("[ZARUFS] block group number is out of group count of sb.\n");
    ZARUFS_ERROR("[ZARUFS] s_groups_count = %lu\n", zsi->s_groups_count);
    ZARUFS_ERROR("[ZARUFS] block_group = %u\n", block_group);
    return NULL;
  }

  gdesc_index = block_group / zsi->s_desc_per_block;
  if (!(group_desc = (struct ext2_group_desc*)(zsi->s_group_desc[gdesc_index]->b_data))) {
    ZARUFS_ERROR("[ZARUFS] cannot find %u th group descriptor\n", block_group);
    return NULL;
  }

  gdesc_offset = block_group % zsi->s_desc_per_block;
  return (group_desc + gdesc_offset);
}

int
zarufs_has_bg_super(struct super_block *sb, int group) {
  if ((ZARUFS_SB(sb)->s_zsb->s_feature_ro_compat &
       EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER) &&
      (!is_group_sparse(group))) {
    return (0);
  }
  return (1);
}

struct buffer_head*
zarufs_get_gdesc_buffer_cache(struct super_block *sb, unsigned int block_group) {
  struct zarufs_sb_info *zsi;
  unsigned long         gdesc_index;

  zsi = ZARUFS_SB(sb);
  if (zsi->s_groups_count <= block_group) {
    ZARUFS_ERROR("[ZARUFS] %s: large block group number.", __func__);
    ZARUFS_ERROR("[ZARUFS] block_group=%u, s_groups_count=%lu\n",
                 block_group, zsi->s_groups_count);
    return(NULL);
  }

  gdesc_index = block_group / zsi->s_desc_per_block;
  return(zsi->s_group_desc[gdesc_index]);
}

unsigned long
zarufs_count_free_blocks(struct super_block *sb) {
  unsigned long desc_count;
  int           i;

  desc_count = 0;
  for (i = 0; i < ZARUFS_SB(sb)->s_groups_count; i++) {
    struct ext2_group_desc *gdesc;
    if (!(gdesc = zarufs_get_group_descriptor(sb, i))) {
      continue;
    }
    desc_count += le16_to_cpu(gdesc->bg_free_blocks_count);
  }
  return (desc_count);
}

unsigned long
zarufs_new_blocks(struct inode *inode,
                  unsigned goal,
                  unsigned long *count,
                  int *err) {
  struct super_block        *sb;
  struct zarufs_sb_info     *zsi;
  struct zarufs_super_block *zsb;
  struct ext2_group_desc    *gdesc;

  struct buffer_head      *bitmap_bh;
  struct buffer_head      *gdesc_bh;

  unsigned long           group_no;
  unsigned long           goal_group;
  unsigned long           free_blocks;
  unsigned long           grp_alloc_blk;
  unsigned long           grp_target_blk;
  unsigned long           ret_block;
  unsigned long           num;
  int                     performed_allocation;

  sb = inode->i_sb;
  zsi = ZARUFS_SB(sb);
  zsb = zsi->s_zsb;

  bitmap_bh = NULL;
  ret_block = 0;
  num       = *count;
  performed_allocation = 0;

  if (!has_free_blocks(zsi)) {
    *err = -ENOSPC;
    goto out;
  }

  /* test whether the goal block is free. */
  if ((goal < le32_to_cpu(zsb->s_first_data_block))
      || le32_to_cpu(zsb->s_blocks_count) <= goal) {
    goal = le32_to_cpu(zsb->s_first_data_block);
  }

  group_no = (goal - le32_to_cpu(zsb->s_first_data_block))
    / zsi->s_blocks_per_group;
  goal_group = group_no;

  if (!(gdesc = zarufs_get_group_descriptor(sb, group_no))) {
    goto io_error;
  }

  if (!(gdesc_bh = zarufs_get_gdesc_buffer_cache(sb, group_no))) {
    goto io_error;
  }

  free_blocks = le16_to_cpu(gdesc->bg_free_blocks_count);
  if (0 < free_blocks) {
    grp_target_blk = (goal - le32_to_cpu(zsb->s_first_data_block))
      % zsi->s_blocks_per_group;
    bitmap_bh = read_block_bitmap(sb, group_no);
    if (!bitmap_bh) {
      goto io_error;
    }
    grp_alloc_blk = try_to_allocate(sb,
                                    group_no,
                                    bitmap_bh,
                                    grp_target_blk,
                                    &num,
                                    NULL);
    if (0 <= grp_alloc_blk) {
      goto allocated;
    }
  } else {
    *err = -ENOSPC;
    goto out;
  }

 allocated:
  DBGPRINT("[ZARUFS] %s: using block group = %lu, free_blocks = %d\n",
           __func__, group_no, gdesc->bg_free_blocks_count);
  ret_block = grp_alloc_blk + zarufs_get_first_block_num(sb, group_no);
  if (IN_RANGE(le32_to_cpu(gdesc->bg_block_bitmap), ret_block, num) ||
      IN_RANGE(le32_to_cpu(gdesc->bg_inode_bitmap), ret_block, num) ||
      IN_RANGE(ret_block, le32_to_cpu(gdesc->bg_inode_table), ZARUFS_SB(sb)->s_itb_per_group)) {
    ZARUFS_ERROR("[ZARUFS] %s: allocating block in system zone -", __func__);
    ZARUFS_ERROR(" block from %lu, length %lu\n", ret_block, num);
    /* as for now, i do not implement retry_alloc. */
    *err = -ENOSPC;
    goto out;
  }

  performed_allocation = 1;
  if (le32_to_cpu(zsb->s_blocks_count) <= (ret_block + num - 1)) {
    ZARUFS_ERROR("[ZARUFS] %s: blocks count(%d) <= block(%lu)", __func__, le32_to_cpu(zsb->s_blocks_count), ret_block);
    goto out;
  }

  adjust_group_blocks(sb, group_no, gdesc, gdesc_bh, -num);
  percpu_counter_sub(&zsi->s_freeblocks_counter, num);

  mark_buffer_dirty(bitmap_bh);

  if (sb->s_flags & MS_SYNCHRONOUS) {
    sync_dirty_buffer(bitmap_bh);
  }

  *err = 0;

  brelse(bitmap_bh);

  if (num < *count) {
    /* dquot_free_block_nodirty(inode, *count - num); */
    mark_inode_dirty(inode);
    *count = num;
  }

  return(ret_block);

 io_error:
  *err = -EIO;

 out:
  if (!performed_allocation) {
    /* dquot_free_block_nodirty(inode, *count); */
    mark_inode_dirty(inode);
  }
  brelse(bitmap_bh);
  return (0);
}

static int
is_group_sparse(int group) {
  if (group <= 1) {
    return (1);
  }
  return (test_root(group, 3) ||
          test_root(group, 5) ||
          test_root(group, 7));
}

static int
test_root(int group, int multiple) {
  int num;
  num = multiple;
  while (num < group) {
    num *= multiple;
  }
  return (num == group);
}

static int
has_free_blocks(struct zarufs_sb_info *zsi) {
  unsigned long free_blocks;
  unsigned long root_blocks;

  free_blocks = percpu_counter_read_positive(&zsi->s_freeblocks_counter);
  root_blocks = le32_to_cpu(zsi->s_zsb->s_r_blocks_count);

  /* if ((free_blocks < (root_blocks + 1)) */
  /*     && !capable(CAP_SYS_RESOURCE) */
  /*     && !uid_eq(zsi->s_resuid, current_fsuid()) */
  /*     && (gid_eq(zsi->s_resgid, GLOBAL_ROOT_GID) */
  /*         || !in_group_p(zsi->s_resgid))) { */
  /*   return (0); */
  /* } */
  if ((free_blocks < (root_blocks + 1))
      && !capable(CAP_SYS_RESOURCE)) {
    return (0);
  }
  return(1);
}

static struct buffer_head*
read_block_bitmap(struct super_block *sb, unsigned long block_group) {
  struct ext2_group_desc *gdesc;
  struct buffer_head     *bh;
  unsigned long          bitmap_blk;

  if (!(gdesc = zarufs_get_group_descriptor(sb, block_group))) {
    return (NULL);
  }

  bitmap_blk = le32_to_cpu(gdesc->bg_block_bitmap);
  bh         = sb_getblk(sb, bitmap_blk);
  if (unlikely(!bh)) {
    ZARUFS_ERROR("[ZARUFS] %s: cannot read block bitmap.", __func__);
    ZARUFS_ERROR("block_group=%lu, block_bitmap=%u\n",
                 block_group, le32_to_cpu(gdesc->bg_block_bitmap));
    return(NULL);
  }

  if (likely(bh_uptodate_or_lock(bh))) {
    return (bh);
  }

  if (bh_submit_read(bh) < 0) {
    brelse(bh);
    ZARUFS_ERROR("[ZARUFS] %s: cannot read block bitmap.", __func__);
    ZARUFS_ERROR("block_group=%lu, block_bitmap=%u\n",
                 block_group, le32_to_cpu(gdesc->bg_block_bitmap));
    return (NULL);
  }

  /* sanity check for bitmap. (here this is just for displaying error.) */
  valid_block_bitmap(sb, gdesc, block_group, bh);
  return(bh);
}

static int
valid_block_bitmap(struct super_block *sb,
                   struct ext2_group_desc *gdesc,
                   unsigned long block_group,
                   struct buffer_head *bh) {
  unsigned long offset;
  unsigned long next_zero_bit;
  unsigned long bitmap_blk;
  unsigned long group_first_block;

  group_first_block = zarufs_get_first_block_num(sb, block_group);
  /* check whether block bitmap block number is set. */
  bitmap_blk = le32_to_cpu(gdesc->bg_block_bitmap);
  offset     = bitmap_blk - group_first_block;
  if (!test_bit_le(offset, bh->b_data)) {
    /* bad block bitmap. */
    goto err_out;
  }

  /* check whether inode bitmap block number is set. */
  bitmap_blk = le32_to_cpu(gdesc->bg_inode_bitmap);
  offset     = bitmap_blk - group_first_block;
  if (!test_bit_le(offset, bh->b_data)) {
    goto err_out;
  }

  /* check whether inode table block number is set. */
  bitmap_blk    = le32_to_cpu(gdesc->bg_inode_table);
  offset        = bitmap_blk - group_first_block;
  next_zero_bit = find_next_zero_bit_le(bh->b_data,
                                        offset + ZARUFS_SB(sb)->s_itb_per_group,
                                        offset);
  if ((offset + ZARUFS_SB(sb)->s_itb_per_group) <= next_zero_bit) {
    /* good bitmap for inode tables. */
    return (1);
  }

 err_out:
  ZARUFS_ERROR("[ZARUFS] %s: invalid block bitmap.\n", __func__);
  ZARUFS_ERROR("block_group=%lu, block=%lu\n", block_group, bitmap_blk);
  return (0);
}

static int
try_to_allocate(struct super_block *sb,
                unsigned long group,
                struct buffer_head *bitmap_bh,
                unsigned long grp_goal,
                unsigned long *count,
                struct ext2_reserve_window *my_rsv) {
  unsigned long start;
  unsigned long end;
  unsigned long num;

  num = 0;
  end = ZARUFS_SB(sb)->s_blocks_per_group;

 repeat:
  start = grp_goal;
  if (ext2_set_bit_atomic(get_sb_blockgroup_lock(ZARUFS_SB(sb), group),
                          grp_goal,
                          bitmap_bh->b_data)) {
    start++;
    grp_goal++;
    if (end <= start) {
      goto fail_access;
    }
    goto repeat;
  }

  num++;

  *count = num;
  return (grp_goal);

 fail_access:
  *count = num;
  return(-1);
}

static void
adjust_group_blocks(struct super_block *sb,
                    unsigned long group_no,
                    struct ext2_group_desc *gdesc,
                    struct buffer_head *bh,
                    long count) {
  if (count) {
    struct zarufs_sb_info *zsi;
    unsigned              free_blocks;

    zsi = ZARUFS_SB(sb);
    spin_lock(get_sb_blockgroup_lock(zsi, group_no));

    free_blocks = le16_to_cpu(gdesc->bg_free_blocks_count);
    gdesc->bg_free_blocks_count = cpu_to_le16(free_blocks + count);

    spin_unlock(get_sb_blockgroup_lock(zsi, group_no));
    mark_buffer_dirty(bh);
  }
}
