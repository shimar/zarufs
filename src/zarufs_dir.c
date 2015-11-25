/* zarufs_dir.c */
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "../include/zarufs.h"
#include "zarufs_dir.h"
#include "zarufs_utils.h"
#include "zarufs_inode.h"

int
zarufs_read_dir(struct file *file, struct dir_context *ctx);

static struct page*
zarufs_get_dir_page_cache(struct inode *inode, unsigned long index);

static inline void
zarufs_put_dir_page_cache(struct page *page);

static unsigned long
zarufs_get_page_last_byte(struct inode *inode, unsigned long page_nr);

static inline unsigned long
get_dir_num_pages(struct inode *inode);

static inline int
prepare_write_block(struct page *page, loff_t pos, unsigned long len);

static inline void
set_dir_entry_type(struct ext2_dir_entry *dent, struct inode *inode);

static int
commit_block_write(struct page *page, loff_t pos, unsigned long len);

#define S_SHIFT 12
static unsigned char zarufs_type_by_mode[S_IFMT >> S_SHIFT] = {
  [S_IFREG  >> S_SHIFT] = EXT2_FT_REG_FILE,
  [S_IFDIR  >> S_SHIFT] = EXT2_FT_DIR,
  [S_IFCHR  >> S_SHIFT] = EXT2_FT_CHRDEV,
  [S_IFBLK  >> S_SHIFT] = EXT2_FT_BLKDEV,
  [S_IFIFO  >> S_SHIFT] = EXT2_FT_FIFO,
  [S_IFSOCK >> S_SHIFT] = EXT2_FT_SOCK,
  [S_IFLNK  >> S_SHIFT] = EXT2_FT_SYMLINK,
};

int
zarufs_read_dir(struct file *file, struct dir_context *ctx) {
  struct super_block       *sb;
  struct inode             *inode;
  unsigned long            offset;
  unsigned long            page_index;
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

  sb     = inode->i_sb;
  offset = ctx->pos & ~PAGE_CACHE_MASK;

  for (page_index = ctx->pos >> PAGE_CACHE_SHIFT;
       page_index < get_dir_num_pages(inode);
       page_index++) {
    struct page           *page;
    struct ext2_dir_entry *dent;
    char                  *start;
    char                  *end;
    
    page = (struct page*) zarufs_get_dir_page_cache(inode, page_index);
    if (IS_ERR(page)) {
      ZARUFS_ERROR("[ZARUFS] bad page in %lu\n", inode->i_ino);
      ctx->pos += PAGE_CACHE_SIZE - offset;
      return(PTR_ERR(page));
    }

    start = (char*) page_address((const struct page*) page);
    end   = start + zarufs_get_page_last_byte(inode, page_index) - (1 + 8 + 3);
    dent  = (struct ext2_dir_entry*) (start + offset);
    while ((char*) dent <= end) {
      unsigned long rec_len;
      unsigned char ftype_index;
      if (!dent->rec_len) {
        ZARUFS_ERROR("[ZARUFS] Error: zero-length directory entry.\n");
        zarufs_put_dir_page_cache(page);
        return(-EIO);
      }

      if (dent->inode) {
        if (EXT2_FT_MAX < dent->file_type) {
          ftype_index = DT_UNKNOWN;
        } else {
          ftype_index = dent->file_type;
        }

        if (!(dir_emit(ctx,
                       dent->name,
                       dent->name_len,
                       le32_to_cpu(dent->inode),
                       ftype_table[ftype_index]))) {
          break;
        }
      }
      /* goto next entry. */
      rec_len = le16_to_cpu(dent->rec_len);
      ctx->pos += rec_len;
      dent = (struct ext2_dir_entry*) ((char*) dent + rec_len);
    }
    zarufs_put_dir_page_cache(page);
  }
  return (0);
}

static struct page*
zarufs_get_dir_page_cache(struct inode *inode, unsigned long index) {
  struct page *page;
  /* read blocks from device and map them. */
  DBGPRINT("page cache inode=%lu\n", (unsigned long) inode->i_ino);
  DBGPRINT("index=%lu\n", index);
  inode->i_mapping->a_ops = &zarufs_aops;
  page = read_mapping_page(inode->i_mapping, index, NULL);
  if (!IS_ERR(page)) {
    kmap(page);
    if (PageError(page)) {
      zarufs_put_dir_page_cache(page);
      return(ERR_PTR(-EIO));
    }
  }
  return (page);
}

static inline void
zarufs_put_dir_page_cache(struct page *page) {
  kunmap(page);
  page_cache_release(page);
}

const struct file_operations zarufs_dir_operations = {
  .iterate = zarufs_read_dir,
};


static inline unsigned long
get_dir_num_pages(struct inode *inode) {
  return ((inode->i_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT);
}

static unsigned long
zarufs_get_page_last_byte(struct inode *inode, unsigned long page_nr) {
  unsigned long last_byte;
  last_byte = inode->i_size - (page_nr << PAGE_CACHE_SHIFT);
  if (last_byte > PAGE_CACHE_SIZE) {
    return PAGE_CACHE_SIZE;
  }
  return (last_byte);
}

ino_t
zarufs_get_ino_by_name(struct inode *dir, struct qstr *child) {
  struct ext2_dir_entry *dent;
  struct page           *page;
  ino_t                 ino;

  dent = zarufs_find_dir_entry(dir, child, &page);
  if (dent) {
    ino = le32_to_cpu(dent->inode);
    zarufs_put_dir_page_cache(page);
  } else {
    ino = 0;
  }
  return(ino);
}

static inline int
zarufs_strcmp(int len, const char* const name, struct ext2_dir_entry *dent) {
  if (len != dent->name_len) {
    return (0);
  }

  if (!dent->inode) {
    return (0);
  }

  return(!memcmp(name, dent->name, len));
}

static inline int
prepare_write_block(struct page *page, loff_t pos, unsigned long len) {
  return(__block_write_begin(page, pos, (unsigned) len, zarufs_get_block));
}

static inline void
set_dir_entry_type(struct ext2_dir_entry *dent, struct inode *inode) {
  struct zarufs_sb_info *zsi;
  umode_t               mode;

  mode = inode->i_mode;
  zsi  = ZARUFS_SB(inode->i_sb);

  if (zsi->s_zsb->s_feature_incompat
      & cpu_to_le32(EXT2_FEATURE_INCOMPAT_FILETYPE)) {
    dent->file_type = zarufs_type_by_mode[ (mode & S_IFMT) >> S_SHIFT ];
  } else {
    dent->file_type = 0;
  }
}

static int
commit_block_write(struct page *page, loff_t pos, unsigned long len) {
  struct address_space *mapping;
  struct inode         *dir;
  int                  err;

  mapping = page->mapping;
  dir     = mapping->host;
  err     = 0;

  /* commit block write. */
  block_write_end(NULL, mapping, pos, len, len, page, NULL);
  if (dir->i_size < (pos + len)) {
    i_size_write(dir, pos + len);
    mark_inode_dirty(dir);
  }

  /* sync file and inode. */
  if (IS_DIRSYNC(dir)) {
    if (!(err = write_one_page(page, 1))) {
      err = sync_inode_metadata(dir, 1);
    }
  } else {
    unlock_page(page);
  }
  return(err);
}

struct ext2_dir_entry*
zarufs_find_dir_entry(struct inode *dir,
                      struct qstr  *child,
                      struct page  **res_page) {
  struct page           *page;
  struct ext2_dir_entry *dent;
  unsigned long         rec_len;
  unsigned long         page_index;
  const char            *name = child->name;
  int                   namelen;

  namelen = child->len;
  rec_len = ZARUFS_DIR_REC_LEN(namelen);

  for (page_index = 0;
       page_index < get_dir_num_pages(dir);
       page_index++) {
    char *start;
    char *end;

    page = (struct page*) zarufs_get_dir_page_cache(dir, page_index);
    if (IS_ERR(page)) {
      ZARUFS_ERROR("[ZARUFS] %s: bad page [%lu]\n", __func__, page_index);
      goto not_found;
    }

    start = (char*) page_address((const struct page*) page);
    end   = start + zarufs_get_page_last_byte(dir, page_index) - rec_len;
    dent  = (struct ext2_dir_entry*) start;
    while ((char*) dent <= end) {
      unsigned long d_rec_len;
      if (dent->rec_len == 0) {
        ZARUFS_ERROR("[ZARUFS] %s: zero-length directory\n", __func__);
        zarufs_put_dir_page_cache(page);
        goto not_found;
      }

      if (zarufs_strcmp(namelen, name, dent)) {
        goto found;
      }

      d_rec_len = le16_to_cpu(dent->rec_len);
      dent = (struct ext2_dir_entry*) ((char*) dent + d_rec_len);
    }
    zarufs_put_dir_page_cache(page);
  }
 not_found:
  return(NULL);

 found:
  *res_page = page;
  return(dent);
}

int
zarufs_make_empty(struct inode *inode, struct inode *parent) {
  struct page           *page;
  struct ext2_dir_entry *dent;
  unsigned long         block_size;
  int                   err;
  void                  *start;

  /* find or create a page for index 0. */
  if (!(page = grab_cache_page(inode->i_mapping, 0))) {
    return (-ENOMEM);
  }

  /* get block size in file system. */
  block_size = inode->i_sb->s_blocksize;
  if ((err = prepare_write_block(page, 0, block_size))) {
    /* failed to prepare. */
    unlock_page(page);
    page_cache_release(page);
    return(err);
  }

  start = kmap_atomic(page);
  memset(start, 0, block_size);
  /* make dot. */
  dent = (struct ext2_dir_entry*) start;
  dent->name_len = 1;
  dent->rec_len  = cpu_to_le16(ZARUFS_DIR_REC_LEN(dent->name_len));
  memcpy(dent->name, ".\0\0", 4);
  dent->inode = cpu_to_le32(inode->i_ino);
  set_dir_entry_type(dent, inode);

  /* make dot dot. */
  dent = (struct ext2_dir_entry*) (start + ZARUFS_DIR_REC_LEN(1));
  dent->name_len = 2;
  dent->rec_len  = cpu_to_le16(block_size - ZARUFS_DIR_REC_LEN(1));
  dent->inode    = cpu_to_le32(parent->i_ino);
  memcpy(dent->name, "..\0", 4);
  set_dir_entry_type(dent, inode);

  kunmap_atomic(start);

  /* commit write block of empty contents. */
  err = commit_block_write(page, 0, block_size);
  page_cache_release(page);

  return(err);
}

int
zarufs_add_link(struct dentry *dentry, struct inode *inode) {
  struct inode          *dir;
  struct page           *page;
  struct ext2_dir_entry *dent;
  const  char           *link_name = dentry->d_name.name;
  int                   link_name_len;
  unsigned long         block_size;
  unsigned long         link_rec_len;
  unsigned short        rec_len;
  unsigned short        name_len;
  unsigned long         page_index;
  loff_t                pos;
  int                   err;

  dir           = dentry->d_parent->d_inode;
  link_name_len = dentry->d_name.len;
  link_rec_len  = ZARUFS_DIR_REC_LEN(link_name_len);
  block_size    = dir->i_sb->s_blocksize;

  /* find entry space in the directory. */
  for (page_index = 0;
       page_index <= get_dir_num_pages(dir);
       page_index++) {
    char *start;
    char *end;
    char *dir_end;

    page = (struct page*) zarufs_get_dir_page_cache(dir, page_index);
    err = PTR_ERR(page);
    if (IS_ERR(page)) {
      ZARUFS_ERROR("[ZARUFS] %s: bad page [%lu]\n", __func__, page_index);
      return (err);
    }

    lock_page(page);
    start = (char*) page_address((const struct page*) page);
    dir_end = start
      + zarufs_get_page_last_byte(dir, page_index);
    dent = (struct ext2_dir_entry*) start;
    end  = start + PAGE_CACHE_SIZE - link_rec_len;

    /* find entry space in the page cache of the directory. */
    while ((char*) dent <= end) {
      if ((char*) dent == dir_end) {
        /* reach i_size */
        name_len      = 0;
        rec_len       = block_size;
        dent->rec_len = cpu_to_le16(rec_len);
        dent->inode   = 0;
        goto got_it;
      }

      /* invalid entry. */
      if (!dent->rec_len) {
        ZARUFS_ERROR("[ZARUFS] %s: zero-length directory entry.\n", __func__);
        err = -EIO;
        goto out_unlock;
      }

      /* the entry already exists. */
      if (zarufs_strcmp(link_name_len, link_name, dent)) {
        err = -EEXIST;
        goto out_unlock;
      }

      name_len = ZARUFS_DIR_REC_LEN(dent->name_len);
      rec_len  = le16_to_cpu(dent->rec_len);

      /* found empty entry. */
      if (!dent->inode && (link_rec_len <= rec_len)) {
        goto got_it;
      }

      /* detected deleted entry. */
      if ((name_len + link_rec_len) <= rec_len) {
        goto got_it;
      }
      dent = (struct ext2_dir_entry*) ((char*) dent + rec_len);
    }
    unlock_page(page);
    zarufs_put_dir_page_cache(page);
  }
  return (-EINVAL);

 got_it:
  pos = page_offset(page)
    + ((char*) dent - (char*) page_address(page));
  if ((err = prepare_write_block(page, pos, rec_len))) {
    goto out_unlock;
  }

  /* insert deleted entry space. */
  if (dent->inode) {
    struct ext2_dir_entry *cur_dent;
    /* here, name_len = rec_len of deleted entry. */
    cur_dent = (struct ext2_dir_entry*) ((char*) dent + name_len);
    cur_dent->rec_len = cpu_to_le16(rec_len - name_len);
    dent->rec_len     = cpu_to_le16(name_len);
    dent              = cur_dent;
  }

  /* finally, link the inode to parent directory. */
  dent->name_len = link_name_len;
  memcpy(dent->name, link_name, link_name_len);
  dent->inode = cpu_to_le32(inode->i_ino);
  set_dir_entry_type(dent, inode);

  err = commit_block_write(page, pos, rec_len);
  dir->i_mtime = CURRENT_TIME_SEC;
  dir->i_ctime = dir->i_mtime;
  ZARUFS_I(dir)->i_flags &= ~EXT2_BTREE_FL;
  mark_inode_dirty(dir);

  zarufs_put_dir_page_cache(page);
  return(err);

 out_unlock:
  unlock_page(page);
  zarufs_put_dir_page_cache(page);
  return(err);
}
