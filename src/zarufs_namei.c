#include <linux/pagemap.h>

#include "../include/zarufs.h"
#include "zarufs_utils.h"
#include "zarufs_inode.h"
#include "zarufs_dir.h"
#include "zarufs_namei.h"
#include "zarufs_ialloc.h"

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
zarufs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode) {
  struct inode *inode;
  int          err;

  DBGPRINT("[ZARUFS] mkdir: start make [%s]\n", dentry->d_name.name);

  /* allocate a new inode for new directory. */
  inode_inc_link_count(dir);
  inode = zarufs_alloc_new_inode(dir, S_IFDIR | mode, &dentry->d_name);
  if (IS_ERR(inode)) {
    inode_dec_link_count(dir);
    return (PTR_ERR(inode));
  }

  inode->i_op             = &zarufs_dir_inode_operations;
  inode->i_fop            = &zarufs_dir_operations;
  inode->i_mapping->a_ops = &zarufs_aops;

  /* make empty directory(just make `.`) */
  inode_inc_link_count(inode);
  if ((err = zarufs_make_empty(inode, dir))) {
    goto out_fail;
  }

  /* insert the new directory's inode to its parent. */
  if ((err = zarufs_add_link(dentry, inode))) {
    goto out_fail;
  }

  unlock_new_inode(inode);
  d_instantiate(dentry, inode);
  DBGPRINT("[ZARUFS] mkdir: complete [%s]\n", dentry->d_name.name);
  return (err);

 out_fail:
  inode_dec_link_count(inode);
  inode_dec_link_count(inode);
  unlock_new_inode(inode);
  iput(inode);
  return(err);
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
  struct inode *inode;
  ino_t        ino;

  if (ZARUFS_NAME_LEN < dentry->d_name.len) {
    return(ERR_PTR(-ENAMETOOLONG));
  }

  DBGPRINT("[ZARUFS] LOOKUP %s!\n", dentry->d_name.name);
  ino = zarufs_get_ino_by_name(dir, &dentry->d_name);

  inode = NULL;
  if (ino) {
    inode = zarufs_get_vfs_inode(dir->i_sb, ino);
    if (inode == ERR_PTR(-ESTALE)) {
      ZARUFS_ERROR("[ZARUFS] %s:deleted inode referenced.\n", __func__);
      return(ERR_PTR(-EIO));
    }
  }
      
  return (d_splice_alias(inode, dentry));
}

const struct inode_operations zarufs_dir_inode_operations = {
  .create  = zarufs_create,
  .lookup  = zarufs_lookup,
  .link    = zarufs_link,
  .unlink  = zarufs_unlink,
  .symlink = zarufs_symlink,
  .mkdir   = zarufs_mkdir,
  .rmdir   = zarufs_rmdir,
  .mknod   = zarufs_mknod,
  .rename  = zarufs_rename,
  .setattr = zarufs_set_attr,
  .get_acl = zarufs_get_acl,
  .tmpfile = zarufs_tmp_file,
};
