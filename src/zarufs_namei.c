#include <linux/pagemap.h>

#include "../include/zarufs.h"
#include "zarufs_utils.h"
#include "zarufs_inode.h"
#include "zarufs_dir.h"
#include "zarufs_namei.h"
#include "zarufs_ialloc.h"
#include "zarufs_file.h"

static int
zarufs_rmdir(struct inode *dir, struct dentry *dentry);

static int
zarufs_unlink(struct inode *dir, struct dentry *dentry);

static int
zarufs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry);

static int
zarufs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);

static inline int
add_non_dir(struct dentry *dentry, struct inode *inode);

static int
zarufs_link(struct dentry *dentry, struct inode *inode, struct dentry *d) {
  DBGPRINT("[ZARUFS] inode ops:link!\n");
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
zarufs_mknod(struct inode *inode, struct dentry *d, umode_t mode, dev_t dev) {
  DBGPRINT("[ZARUFS] inode ops:mknod!\n");
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


static int
zarufs_rmdir(struct inode *dir, struct dentry *dentry) {
  struct inode *inode;
  int          err;

  err = -ENOTEMPTY;
  inode = dentry->d_inode;

  if (zarufs_is_empty_dir(inode)) {
    if (!(err = zarufs_unlink(dir, dentry))) {
      inode->i_size = 0;
      inode_dec_link_count(inode);
      inode_dec_link_count(dir);
    }
  }

  return(err);
}

static int
zarufs_unlink(struct inode *dir, struct dentry *dentry) {
  struct inode          *inode;
  struct ext2_dir_entry *dent;
  struct page           *page;
  int                   err;

  if (!(dent = zarufs_find_dir_entry(dir, &dentry->d_name, &page))) {
    return (-ENOENT);
  }

  if ((err = zarufs_delete_dir_entry(dent, page))) {
    return (err);
  }

  inode = dentry->d_inode;
  inode->i_ctime = dir->i_ctime;
  inode_dec_link_count(inode);

  return (0);
}

static int
zarufs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry) {
  struct inode          *old_inode;
  struct page           *old_page;
  struct ext2_dir_entry *old_dent;
  struct inode          *new_inode;
  struct page           *dir_page;
  struct ext2_dir_entry *dir_dent;

  int err;

  old_inode = old_dentry->d_inode;
  old_dent  = zarufs_find_dir_entry(old_dir, &old_dentry->d_name, &old_page);
  new_inode = new_dentry->d_inode;
  dir_page  = NULL;
  dir_dent  = NULL;
  err = -EIO;
  if (!old_dent) {
    goto out;
  }

  if (S_ISDIR(old_inode->i_mode)) {
    dir_dent = zarufs_get_dot_dot_entry(old_inode, &dir_page);
    if (!dir_dent) {
      goto out_old;
    }
  }

  if (new_inode) {
    struct page           *new_page;
    struct ext2_dir_entry *new_dent;
    if (dir_dent && !zarufs_is_empty_dir(new_inode)) {
      err = !ENOTEMPTY;
      goto out_dir;
    }
    new_dent = zarufs_find_dir_entry(new_dir, &new_dentry->d_name, &new_page);
    if (!new_dent) {
      err = -ENOENT;
      goto out_dir;
    }
    zarufs_set_link(new_dir, new_dent, new_page, old_inode, 1);
    new_inode->i_ctime = CURRENT_TIME_SEC;
    if (dir_dent) {
      drop_nlink(new_inode);
    }
    inode_dec_link_count(new_inode);
  } else {
    if ((err = zarufs_add_link(new_dentry, old_inode))) {
      goto out_dir;
    }
    if (dir_dent) {
      inode_inc_link_count(new_dir);
    }
  }

  old_inode->i_ctime = CURRENT_TIME_SEC;
  mark_inode_dirty(old_inode);
  zarufs_delete_dir_entry(old_dent, old_page);

  if (dir_dent) {
    if (old_dir != new_dir) {
      zarufs_set_link(old_inode, dir_dent, dir_page, new_dir, 0);
    } else {
      kunmap(dir_page);
      page_cache_release(dir_page);
    }
    inode_dec_link_count(old_dir);
  }
  return (0);

 out_dir:
  if (dir_dent) {
    kunmap(dir_page);
    page_cache_release(dir_page);
  }

 out_old:
  kunmap(old_page);
  page_cache_release(old_page);

 out:
  return(err);
}

static int
zarufs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
  struct inode *inode;
  DBGPRINT("[ZARUFS] %s: create [%s]\n", __func__, dentry->d_name.name);

  inode = zarufs_alloc_new_inode(dir, mode, &dentry->d_name);

  if (IS_ERR(inode)) {
    DBGPRINT("[ZARUFS] %s: faild to create[%s]\n", __func__, dentry->d_name.name);
    return(PTR_ERR(inode));
  }

  inode->i_op = &zarufs_file_inode_operations;
  inode->i_mapping->a_ops = &zarufs_aops;
  inode->i_fop = &zarufs_file_operations;

  mark_inode_dirty(inode);
  return (add_non_dir(dentry, inode));
}

static inline int
add_non_dir(struct dentry *dentry, struct inode *inode) {
  int err;

  if (!(err = zarufs_add_link(dentry, inode))) {
    DBGPRINT("[ZARUFS] %s:create [%s]\n", __func__, dentry->d_name.name);
    unlock_new_inode(inode);
    d_instantiate(dentry, inode);
    return(0);
  }

  DBGPRINT("[ZARUFS] %s:error add non-dir entry.\n", __func__);
  inode_dec_link_count(inode);
  unlock_new_inode(inode);
  iput(inode);
  return(err);
}
