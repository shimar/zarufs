/* zarufs_dir.c */
#include <linux/fs.h>
#include "../include/zarufs.h"
#include "zarufs_utils.h"

int
zarufs_read_dir(struct file *file, struct dir_context *ctx);

struct dentry*
zarufs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);

int
zarufs_read_dir(struct file *file, struct dir_context *ctx) {
  if (ctx->pos == 0) {
    DBGPRINT("[ZARUFS] Read Dir .(dot)!\n");
    if (!dir_emit_dot(file, ctx)) {
      ZARUFS_ERROR("[ZARUFS] Cannot emit\".\" directory entry.\n");
      return (-1);
    }
    ctx->pos = 1;
  }
  if (ctx->pos == 1) {
    DBGPRINT("[ZARUFS] Read Dir ..(dot dot)!\n");
    if (!dir_emit_dotdot(file, ctx)) {
      ZARUFS_ERROR("[ZARUFS] Cannot emit\"..\" directory entry.\n");
      return (-1);
    }
    ctx->pos = 2;
  }
  return (0);
}

struct file_operations zarufs_dir_operations = {
  .iterate = zarufs_read_dir,
};

static int
zarufs_create(struct inode *inode, struct dentry *dir, umode_t mode, bool flag) {
  DBGPRINT("[ZARUFS] inode ops:create!\n");
  return (0);
}

static int
zarufs_link(struct dentry *dentry, struct inode *inode, struct dentry *d) {
  DBGPRINT("[ZARUFS] inode ops:link!\n");
  return (0);
}

static int
zarufs_unlink(struct inode *inode, struct dentry *d) {
  DBGPRINT("[ZARUFS] inode ops:unlink!\n");
  return (0);
}

static int
zarufs_symlink(struct inode *inode, struct dentry *d, const char *name) {
  DBGPRINT("[ZARUFS] inode ops:symlink!\n");
  return (0);
}

static int
zarufs_mkdir(struct inode *inode, struct dentry *d, umode_t mode) {
  DBGPRINT("[ZARUFS] inode ops:mkdir!\n");
  return (0);
}

static int
zarufs_rmdir(struct inode *inode, struct dentry *d) {
  DBGPRINT("[ZARUFS] inode ops:rmdir!\n");
  return (0);
}

static int
zarufs_mknod(struct inode *inode, struct dentry *d, umode_t mode, dev_t dev) {
  DBGPRINT("[ZARUFS] inode ops:mknod!\n");
  return (0);
}

static int
zarufs_rename(struct inode *inode, struct dentry *d, struct inode *inode2, struct dentry *d2) {
  DBGPRINT("[ZARUFS] inode ops:rename!\n");
  return (0);
}

static int
zarufs_set_attr(struct dentry *dentry, struct iattr *attr) {
  DBGPRINT("[ZARUFS] inode ops:set_attr!\n");
  return (0);
}

static struct posix_acl*
zarufs_get_acl(struct inode *inode, int flags) {
  DBGPRINT("[ZARUFS] inode ops:get_acl!\n");
  return (0);
}

static int
zarufs_tmp_file(struct inode *inode, struct dentry *d, umode_t mode) {
  DBGPRINT("[ZARUFS] inode ops:tmp_file!\n");
  return (0);
}


struct dentry*
zarufs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags) {
  DBGPRINT("[ZARUFS] LOOKUP in directory!\n");
  return (NULL);
}

struct inode_operations zarufs_dir_inode_operations = {
  .create = zarufs_create,
  .lookup = zarufs_lookup,
  .link   = zarufs_link,
  .unlink = zarufs_unlink,
  .symlink = zarufs_symlink,
  .mkdir = zarufs_mkdir,
  .rmdir = zarufs_rmdir,
  .mknod = zarufs_mknod,
  .rename = zarufs_rename,
  .setattr = zarufs_set_attr,
  .get_acl = zarufs_get_acl,
  .tmpfile = zarufs_tmp_file,
};
