#include <linux/fs.h>

#include "../include/zarufs.h"
#include "zarufs_utils.h"

const struct file_operations  zarufs_file_operations = {
  .llseek       = generic_file_llseek,
  .read         = new_sync_read,
  .write        = new_sync_write,
  .read_iter    = generic_file_read_iter,
  .write_iter   = generic_file_write_iter,
  .mmap         = generic_file_mmap,
  .open         = generic_file_open,
  .fsync        = generic_file_fsync,
  .splice_read  = generic_file_splice_read,
  .splice_write = iter_file_splice_write,
};
const struct inode_operations zarufs_file_inode_operations;
