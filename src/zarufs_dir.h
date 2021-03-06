/* zarufs_dir.h */
#ifndef _ZARUFS_DIR_H_
#define _ZARUFS_DIR_H_

extern const struct file_operations  zarufs_dir_operations;
extern const struct address_space_operations zarufs_aops;

int
zarufs_read_dir(struct file *file, struct dir_context *ctx);

ino_t
zarufs_get_ino_by_name(struct inode *dir, struct qstr *child);

struct ext2_dir_entry*
zarufs_find_dir_entry(struct inode *dir,
                      struct qstr  *child,
                      struct page  **res_page);

int
zarufs_make_empty(struct inode *inode, struct inode *parent);

int
zarufs_add_link(struct dentry *dentry, struct inode *inode);

int
zarufs_is_empty_dir(struct inode *inode);

int
zarufs_delete_dir_entry(struct ext2_dir_entry *dir, struct page *page);

struct ext2_dir_entry*
zarufs_get_dot_dot_entry(struct inode *dir, struct page **p);

void
zarufs_set_link(struct inode *dir,
                struct ext2_dir_entry *dent,
                struct page *page,
                struct inode *inode,
                int update_times);

#endif
