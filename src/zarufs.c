/* zarufs.c */
#include <linux/module.h>
#include <linux/fs.h>

#include "../include/zarufs.h"
#include "zarufs_utils.h"

static struct dentry *zarufs_mount(struct file_system_type *fs_type,
                                   int flags,
                                   const char *dev_name,
                                   void *data);


static struct file_system_type zarufs_fstype = {
  .name     = "zarufs",
  .mount    = zarufs_mount,
  .fs_flags = FS_REQUIRES_DEV,
};


static struct dentry *zarufs_mount(struct file_system_type *fs_type,
                                   int flags,
                                   const char *dev_name,
                                   void *data) {

  DBGPRINT("[ZARUFS] Not Implemented Yet!\n");
  return NULL;
}

static int __init init_zarufs(void) {
  DBGPRINT("[ZARUFS] Hello, World.\n");
  return register_filesystem(&zarufs_fstype);
}

static void __exit exit_zarufs(void) {
  DBGPRINT("[ZARUFS] GoodBye!.\n");
  unregister_filesystem(&zarufs_fstype);
  return;
}

module_init(init_zarufs);
module_exit(exit_zarufs);

MODULE_LICENSE("GPL");
