#ifndef _ZARUFS_SUPER_H_
#define _ZARUFS_SUPER_H_

struct dentry *
zarufs_mount_block_dev(struct file_system_type *fs_type,
                       int flags,
                       const char *dev_name,
                       void *data);
#endif
