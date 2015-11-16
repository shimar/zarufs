/* zarufs_dir.c */
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "../include/zarufs.h"
#include "zarufs_utils.h"

int
zarufs_read_dir(struct file *file, struct dir_context *ctx);

struct dentry*
zarufs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);

int
zarufs_read_dir(struct file *file, struct dir_context *ctx) {
  struct super_block       *sb;
  struct inode             *inode;
  struct zarufs_inode_info *zi;
  struct ext2_dir_entry    *dent;
  struct buffer_head       *bh;
  unsigned long            offset;
  unsigned long            block_index;
  unsigned long            read_len;
  unsigned char            ftype_table[EXT2_FT_MAX] = {
    [ EXT2_FT_UNKNOWN ]  = DT_UNKNOWN,
    [ EXT2_FT_REG_FILE ] = DT_REG,
    [ EXT2_FT_DIR ]      = DT_DIR,
    [ EXT2_FT_CHRDEV ]   = DT_CHR,
    [ EXT2_FT_BLKDEV ]   = DT_BLK,
    [ EXT2_FT_FIFO   ]   = DT_FIFO,
    [ EXT2_FT_SOCK   ]   = DT_SOCK,
    [ EXT2_FT_SYMLINK ]  = DT_LNK,
  };

  inode = file_inode(file);
  /* whether position exceeds last of minimum dir entry or not. */
  if ((inode->i_size - (1 + 8 + 3)) < ctx->pos) {
    /* there is no more entry in the directory. */
    return (0);
  }

  sb = inode->i_sb;
  /* calculate offset in directory page caches.  */
  block_index = ctx->pos >> sb->s_blocksize_bits;
  offset      = ctx->pos & ~((1 << sb->s_blocksize_bits) - 1);

  bh = NULL;
  if (block_index < ZARUFS_IND_BLOCK) {
    zi = ZARUFS_I(inode);
    if (!(bh = sb_bread(sb, zi->i_data[block_index]))) {
      ZARUFS_ERROR("[ZARUFS] Error: cannot read block %d\n", zi->i_data[block_index]);
      return (-1);
    }
  } else if (offset < ZARUFS_2IND_BLOCK) {
    DBGPRINT("[ZARUFS] UNIMPLEMENTED INDIRECT BLOCK!\n");
    return (-1);
  }

  DBGPRINT("[ZARUFS] Fill Dir entries!\n");

  dent = (struct ext2_dir_entry*)(bh->b_data + offset);
  read_len = 1 + 8 + 3;
  DBGPRINT("[ZARUFS] inode size = %llu\n", (unsigned long long) inode->i_size);

  for (;;) {
    if (!dent->rec_len) {
      DBGPRINT("[ZARUFS] Fill dir is over!\n");
      break;
    }
    if (((inode->i_size - (1 + 8 + 3)) < ctx->pos) ||
        (sb->s_blocksize <= (read_len - (1 + 8 + 3)))) {
      DBGPRINT("[ZARUFS] Fill dir is over!\n");
      break;
    }
    if (dent->inode) {
      unsigned char ftype_index;
      int           i;
      DBGPRINT("[ZARUFS] [fill dirent]    inode=%u\n", le32_to_cpu(dent->inode));
      DBGPRINT("[ZARUFS] [fill dirent] name_len=%d\n", dent->name_len);
      DBGPRINT("[ZARUFS] [fill dirent]  rec_len=%u\n", le16_to_cpu(dent->rec_len));
      DBGPRINT("[ZARUFS] [fill dirent] read_len=%lu\n", read_len);
      DBGPRINT("[ZARUFS] [fill dirent] ctx->pos=%llu\n", (unsigned long long) ctx->pos);
      DBGPRINT("[ZARUFS] [fill dirent]     name=\"");
      for (i = 0; i < dent->name_len; i++) {
        DBGPRINT("%c", dent->name[i]);
      }
      DBGPRINT("\"\n");
      if (EXT2_FT_MAX < dent->file_type) {
        ftype_index = DT_UNKNOWN;
      } else {
        ftype_index = dent->file_type;
      }

      if (!(dir_emit(ctx, dent->name, dent->name_len, le32_to_cpu(dent->inode), ftype_table[ftype_index]))) {
        break;
      }
      dent = (struct ext2_dir_entry*) ((unsigned char*) dent + dent->rec_len);
    }
    ctx->pos += le16_to_cpu(dent->rec_len);
    read_len += le16_to_cpu(dent->rec_len);
  }
  brelse(bh);
  
  /* if (ctx->pos == 0) { */
  /*   DBGPRINT("[ZARUFS] Read Dir .(dot)!\n"); */
  /*   if (!dir_emit_dot(file, ctx)) { */
  /*     ZARUFS_ERROR("[ZARUFS] Cannot emit\".\" directory entry.\n"); */
  /*     return (-1); */
  /*   } */
  /*   ctx->pos = 1; */
  /* } */
  /* if (ctx->pos == 1) { */
  /*   DBGPRINT("[ZARUFS] Read Dir ..(dot dot)!\n"); */
  /*   if (!dir_emit_dotdot(file, ctx)) { */
  /*     ZARUFS_ERROR("[ZARUFS] Cannot emit\"..\" directory entry.\n"); */
  /*     return (-1); */
  /*   } */
  /*   ctx->pos = 2; */
  /* } */
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
