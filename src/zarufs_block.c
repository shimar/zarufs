/* zarufs_block.c */
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "../include/zarufs.h"
#include "zarufs_block.h"
#include "zarufs_utils.h"

static int is_group_sparse(int group);
static int test_root(int group, int multiple);

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
