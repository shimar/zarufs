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

void
zarufs_set_vfs_inode_flags(struct inode *inode);

/* -------------------------------------------------------------------------- */

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

struct inode
*zarufs_get_vfs_inode(struct super_block *sb, unsigned int ino) {
  struct inode             *inode;
  struct zarufs_inode_info *zi;
  struct ext2_inode        *ext2_inode;
  struct buffer_head       *bh;
  uid_t                    i_uid;
  gid_t                    i_gid;
  int                      i;

  /* find an inode cache or allocate inode and lock it. */
  inode = iget_locked(sb, ino);
  if (!inode) {
    return (ERR_PTR(-ENOMEM));
  }

  /* get an inode which has already exists. */
  if (!(inode->i_state & I_NEW)) {
    return (inode);
  }

  /* read ext2 inode. */
  zi = ZARUFS_I(inode);
  ext2_inode = zarufs_get_ext2_inode(inode->i_sb, ino, &bh);
  if (IS_ERR(ext2_inode)) {
    iget_failed(inode);
    return ((struct inode*)(ext2_inode));
  }

  /* debug_print_ext2_inode_info(ext2_inode); */

  /* setup vfs inode. */
  inode->i_mode = le16_to_cpu(ext2_inode->i_mode);
  i_uid         = (uid_t) le16_to_cpu(ext2_inode->i_uid);
  i_gid         = (gid_t) le16_to_cpu(ext2_inode->i_gid);
  if (ZARUFS_SB(sb)->s_mount_opt & EXT2_MOUNT_NO_UID32) {
    /* NOP */
  } else {
    i_uid |= le16_to_cpu(ext2_inode->osd2.linux2.l_i_uid_high);
    i_gid |= le16_to_cpu(ext2_inode->osd2.linux2.l_i_gid_high);
  }
  i_uid_write(inode, i_uid);
  i_gid_write(inode, i_gid);
  set_nlink(inode, le16_to_cpu(ext2_inode->i_links_count));
  inode->i_size = le32_to_cpu(ext2_inode->i_size);
  inode->i_atime.tv_sec = (signed int) le32_to_cpu(ext2_inode->i_atime);
  inode->i_ctime.tv_sec = (signed int) le32_to_cpu(ext2_inode->i_ctime);
  inode->i_mtime.tv_sec = (signed int) le32_to_cpu(ext2_inode->i_mtime);
  inode->i_atime.tv_nsec = 0;
  inode->i_ctime.tv_nsec = 0;
  inode->i_mtime.tv_nsec = 0;
  zi->i_dtime = le32_to_cpu(ext2_inode->i_dtime);

  if ((inode->i_nlink == 0) &&
      ((inode->i_mode == 0) || (zi->i_dtime))) {
    brelse(bh);
    iget_failed(inode);
    return (ERR_PTR(-ESTALE));
  }

  inode->i_blocks = le32_to_cpu(ext2_inode->i_blocks);
  zi->i_flags     = le32_to_cpu(ext2_inode->i_flags);
  zi->i_faddr     = le32_to_cpu(ext2_inode->i_faddr);
  zi->i_frag_no   = ext2_inode->osd2.linux2.l_i_frag;
  zi->i_frag_size = ext2_inode->osd2.linux2.l_i_fsize;
  zi->i_file_acl  = le32_to_cpu(ext2_inode->i_file_acl);
  zi->i_dir_acl = 0;
  if (S_ISREG(inode->i_mode)) {
    inode->i_size |= ((__u64)le32_to_cpu(ext2_inode->i_dir_acl)) << 32;
  } else {
    zi->i_dir_acl = le32_to_cpu(ext2_inode->i_dir_acl);
  }
  inode->i_generation = le32_to_cpu(ext2_inode->i_generation);
  zi->i_dtime            = 0;
  zi->i_state            = 0;
  zi->i_block_group      = (ino - 1) / ZARUFS_SB(sb)->s_inodes_per_group;
  zi->i_dir_start_lookup = 0;

  for (i = 0; i < ZARUFS_NR_BLOCKS; i++) {
    zi->i_data[i] = ext2_inode->i_block[i];
  }

  if (S_ISREG(inode->i_mode)) {
  } else if (S_ISDIR(inode->i_mode)) {
  } else if (S_ISLNK(inode->i_mode)) {
  } else {
  }

  brelse(bh);
  zarufs_set_vfs_inode_flags(inode);
  /* debug_print_zarufs_inode_info(zi); */
  /* debug_print_vfs_inode(inode); */
  return (inode);
}

void
zarufs_set_vfs_inode_flags(struct inode *inode) {
  unsigned int flags;
  flags = ZARUFS_I(inode)->i_flags;
  inode->i_flags &= ~(S_SYNC | S_APPEND | S_IMMUTABLE | S_NOATIME | S_DIRSYNC );
  if (flags & EXT2_SYNC_FL) {
    inode->i_flags |= S_SYNC;
  }
  if (flags & EXT2_APPEND_FL) {
    inode->i_flags |= S_APPEND;
  }
  if (flags & EXT2_IMMUTABLE_FL) {
    inode->i_flags |= S_IMMUTABLE;
  }
  if (flags & EXT2_NOATIME_FL) {
    inode->i_flags |= S_NOATIME;
  }
  if (flags & EXT2_DIRSYNC_FL) {
    inode->i_flags |= S_DIRSYNC;
  }
}
