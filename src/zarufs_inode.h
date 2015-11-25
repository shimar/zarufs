/* zarufs_inode.h */
#ifndef _ZARUFS_INODE_H_
#define _ZARUFS_INODE_H_

int
zarufs_get_block(struct inode *inode,
                 sector_t iblock,
                 struct buffer_head *bh_result,
                 int create);

void
zarufs_set_vfs_inode_flags(struct inode *inode);

struct inode
*zarufs_get_vfs_inode(struct super_block *sb, unsigned int ino);

#endif
