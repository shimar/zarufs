/* zarufs_dir.h */
#ifndef _ZARUFS_DIR_H_
#define _ZARUFS_DIR_H_

static int
zarufs_read_dir(struct file *file, struct dir_context *ctx);

static struct dentry*
zarufs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);

#endif
