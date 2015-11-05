/* zarufs_inode.c */
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "../include/zarufs.h"
#include "zarufs_utils.h"
#include "zarufs_block.h"
#include "zarufs_inode.h"

static struct ext2_inode*
zarufs_get_ext2_inode(struct super_block *sb,
                      unsigned long ino,
                      struct buffer_head **bhp);

static struct ext2_inode*
zarufs_get_ext2_inode(struct super_block *sb,
                      unsigned long ino,
                      struct buffer_head **bhp) {
  struct ext2_group_desc *gdesc;
  unsigned long          block_group;
  unsigned long          block_offset;
  unsigned long          inode_index;
  unsigned long          inode_block;

  *bhp = NULL;

  /* sanity check for inode number. */
  if ((ino != ZARUFS_EXT2_ROOT_INO) &&
      (ino < ZARUFS_SB(sb)->s_first_ino)) {
    ZARUFS_ERROR("[ZARUFS] Error: failed to get ext2 inode[1](ino=%lu)\n", ino);
    return (ERR_PTR(-EINVAL));
  }

  if (le32_to_cpu(ZARUFS_SB(sb)->s_zsb->s_inodes_count) < ino) {
    ZARUFS_ERROR("[ZARUFS] Error: failed to get ext2 inode[2](ino=%lu)\n", ino);
    return (ERR_PTR(-EINVAL));
  }

  /* get group descriptor. */
  block_group = (ino - 1) / ZARUFS_SB(sb)->s_inodes_per_group;
  gdesc = (struct ext2_group_desc*) zarufs_get_group_descriptor(sb, block_group);
  if (!gdesc) {
    ZARUFS_ERROR("[ZARUFS] Error: cannot find group descriptor(ino=%lu)\n", ino);
    return (ERR_PTR(-EIO));
  }

  /* get inode block number and read it. */
  block_offset = ((ino - 1) % ZARUFS_SB(sb)->s_inodes_per_group)
    * ZARUFS_SB(sb)->s_inode_size;
  inode_index = block_offset >> sb->s_blocksize_bits;
  inode_block = le32_to_cpu(gdesc->bg_inode_table) + inode_index;
  if (!(*bhp = sb_bread(sb, inode_block))) {
    ZARUFS_ERROR("[ZARUFS] Error: unable to read inode block[1].\n");
    ZARUFS_ERROR("[ZARUFS] (ino=%lu)\n", ino);
    return (ERR_PTR(-EIO));
  }
  block_offset &= (sb->s_blocksize - 1);
  return ((struct ext2_inode*)((*bhp)->b_data + block_offset));
}
