/* zarufs_dir.h */
#ifndef _ZARUFS_DIR_H_
#define _ZARUFS_DIR_H_

extern const struct file_operations  zarufs_dir_operations;
extern const struct inode_operations zarufs_dir_inode_operations;
extern const struct address_space_operations zarufs_aops;

int
zarufs_read_dir(struct file *file, struct dir_context *ctx);

struct dentry*
zarufs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);

#endif
