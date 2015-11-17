/* zarufs_dir.c */
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "../include/zarufs.h"
#include "zarufs_dir.h"
#include "zarufs_utils.h"

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
