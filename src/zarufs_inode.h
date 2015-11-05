/* zarufs_inode.h */
#ifndef _ZARUFS_INODE_H_
#define _ZARUFS_INODE_H_

struct inode
*zarufs_get_vfs_inode(struct super_block *sb, unsigned int ino);

#endif
