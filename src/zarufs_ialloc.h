#ifndef _ZARUFS_IALLOC_H_
#define _ZARUFS_IALLOC_H_

struct inode*
zarufs_alloc_new_inode(struct inode *dir, umode_t mode, const struct qstr *qstr);

unsigned long
zarufs_count_free_inodes(struct super_block *sb);

unsigned long
zarufs_count_directories(struct super_block *sb);

#endif
