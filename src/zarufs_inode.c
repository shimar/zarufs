/* zarufs_inode.c */
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/mpage.h>
#include <linux/sched.h>
#include <linux/writeback.h>

#include "../include/zarufs.h"
#include "zarufs_utils.h"
#include "zarufs_block.h"
#include "zarufs_inode.h"
#include "zarufs_dir.h"
#include "zarufs_namei.h"

typedef struct {
  __le32             *p;
  __le32             key;
  struct buffer_head *bh;
} indirect;

static struct ext2_inode*
zarufs_get_ext2_inode(struct super_block *sb,
                      unsigned long ino,
                      struct buffer_head **bhp);

static int
zarufs_get_blocks(struct inode *inode,
                  sector_t iblock,
                  unsigned long maxblocks,
                  struct buffer_head *bh_result,
                  int create);

static int
zarufs_block_to_path(struct inode *inode,
                     unsigned long i_block,
                     int offsets[4],
                     int *boundary);

static int
zarufs_read_page(struct file *filp, struct page *page);

static int
zarufs_read_pages(struct file *filp,
                  struct address_space *mapping,
                  struct list_head *pages,
                  unsigned nr_pages);

static inline void
add_chain(indirect *p, struct buffer_head *bh, __le32 *v);

static indirect*
zarufs_get_branch(struct inode *inode,
                  int          depth,
                  int          *offsets,
                  indirect     chain[4],
                  int          *err);

static int
zarufs_write_page(struct page *page, struct writeback_control *wbc);

static int
zarufs_write_begin(struct file *file,
                   struct address_space *mapping,
                   loff_t pos,
                   unsigned len,
                   unsigned flags,
                   struct page **pagep,
                   void **fsdata);

static int
zarufs_write_end(struct file *file,
                 struct address_space *mapping,
                 loff_t pos,
                 unsigned len,
                 unsigned copied,
                 struct page *pagep,
                 void *fsdata);

static int
__zarufs_write_inode(struct inode *inode, int do_sync);

static sector_t
zarufs_bmap(struct address_space *mapping, sector_t sec) {
  DBGPRINT("[ZARUFS] AOPS:bmap!\n");
  return(0);
}

static int
zarufs_write_pages(struct address_space *mapping,
                   struct writeback_control *wbc);

static inline unsigned long
find_goal(struct inode *inode, long block, indirect *partial);

static inline unsigned long
find_near(struct inode *inode, indirect *ind);

static int
alloc_branch(struct inode *inode,
             int indirect_blks,
             int *blks,
             unsigned long goal,
             int *offsets,
             indirect *branch);

static void
splice_branch(struct inode *inode,
              long block,
              indirect *where,
              int num,
              int blks);

static inline int
verify_indirect_chain(indirect *from, indirect *to);

const struct address_space_operations zarufs_aops = {
  .readpage              = zarufs_read_page,
  .readpages             = zarufs_read_pages,
  .writepage             = zarufs_write_page,
  .write_begin           = zarufs_write_begin,
  .write_end             = zarufs_write_end,
  .bmap                  = zarufs_bmap,
  .writepages            = zarufs_write_pages,
  .migratepage           = buffer_migrate_page,
  .is_partially_uptodate = block_is_partially_uptodate,
  .error_remove_page     = generic_error_remove_page,
};

/* -------------------------------------------------------------------------- */

static struct ext2_inode*
zarufs_get_ext2_inode(struct super_block *sb,
                      unsigned long ino,
                      struct buffer_head **bhp) {
  struct ext2_group_desc *gdesc;
  unsigned long          block_group;
  unsigned long          block_offset;
  unsigned long          inode_index;
  unsigned long          inode_block;

  *bhp = NULL;

  /* sanity check for inode number. */
  if ((ino != ZARUFS_EXT2_ROOT_INO) &&
      (ino < ZARUFS_SB(sb)->s_first_ino)) {
    ZARUFS_ERROR("[ZARUFS] Error: failed to get ext2 inode[1](ino=%lu)\n", ino);
    return (ERR_PTR(-EINVAL));
  }

  if (le32_to_cpu(ZARUFS_SB(sb)->s_zsb->s_inodes_count) < ino) {
    ZARUFS_ERROR("[ZARUFS] Error: failed to get ext2 inode[2](ino=%lu)\n", ino);
    return (ERR_PTR(-EINVAL));
  }

  /* get group descriptor. */
  block_group = (ino - 1) / ZARUFS_SB(sb)->s_inodes_per_group;
  gdesc = (struct ext2_group_desc*) zarufs_get_group_descriptor(sb, block_group);
  if (!gdesc) {
    ZARUFS_ERROR("[ZARUFS] Error: cannot find group descriptor(ino=%lu)\n", ino);
    return (ERR_PTR(-EIO));
  }

  /* get inode block number and read it. */
  block_offset = ((ino - 1) % ZARUFS_SB(sb)->s_inodes_per_group)
    * ZARUFS_SB(sb)->s_inode_size;
  inode_index = block_offset >> sb->s_blocksize_bits;
  inode_block = le32_to_cpu(gdesc->bg_inode_table) + inode_index;
  if (!(*bhp = sb_bread(sb, inode_block))) {
    ZARUFS_ERROR("[ZARUFS] Error: unable to read inode block[1].\n");
    ZARUFS_ERROR("[ZARUFS] (ino=%lu)\n", ino);
    return (ERR_PTR(-EIO));
  }
  block_offset &= (sb->s_blocksize - 1);
  return ((struct ext2_inode*)((*bhp)->b_data + block_offset));
}

struct inode
*zarufs_get_vfs_inode(struct super_block *sb, unsigned int ino) {
  struct inode             *inode;
  struct zarufs_inode_info *zi;
  struct ext2_inode        *ext2_inode;
  struct buffer_head       *bh;
  uid_t                    i_uid;
  gid_t                    i_gid;
  int                      i;

  /* find an inode cache or allocate inode and lock it. */
  inode = iget_locked(sb, ino);
  if (!inode) {
    return (ERR_PTR(-ENOMEM));
  }

  /* get an inode which has already exists. */
  if (!(inode->i_state & I_NEW)) {
    return (inode);
  }

  /* read ext2 inode. */
  zi = ZARUFS_I(inode);
  ext2_inode = zarufs_get_ext2_inode(inode->i_sb, ino, &bh);
  if (IS_ERR(ext2_inode)) {
    iget_failed(inode);
    return ((struct inode*)(ext2_inode));
  }

  /* debug_print_ext2_inode_info(ext2_inode); */

  /* setup vfs inode. */
  inode->i_mode = le16_to_cpu(ext2_inode->i_mode);
  i_uid         = (uid_t) le16_to_cpu(ext2_inode->i_uid);
  i_gid         = (gid_t) le16_to_cpu(ext2_inode->i_gid);
  if (ZARUFS_SB(sb)->s_mount_opt & EXT2_MOUNT_NO_UID32) {
    /* NOP */
  } else {
    i_uid |= le16_to_cpu(ext2_inode->osd2.linux2.l_i_uid_high);
    i_gid |= le16_to_cpu(ext2_inode->osd2.linux2.l_i_gid_high);
  }
  i_uid_write(inode, i_uid);
  i_gid_write(inode, i_gid);
  set_nlink(inode, le16_to_cpu(ext2_inode->i_links_count));
  inode->i_size = le32_to_cpu(ext2_inode->i_size);
  inode->i_atime.tv_sec = (signed int) le32_to_cpu(ext2_inode->i_atime);
  inode->i_ctime.tv_sec = (signed int) le32_to_cpu(ext2_inode->i_ctime);
  inode->i_mtime.tv_sec = (signed int) le32_to_cpu(ext2_inode->i_mtime);
  inode->i_atime.tv_nsec = 0;
  inode->i_ctime.tv_nsec = 0;
  inode->i_mtime.tv_nsec = 0;
  zi->i_dtime = le32_to_cpu(ext2_inode->i_dtime);

  if ((inode->i_nlink == 0) &&
      ((inode->i_mode == 0) || (zi->i_dtime))) {
    brelse(bh);
    iget_failed(inode);
    return (ERR_PTR(-ESTALE));
  }

  inode->i_blocks = le32_to_cpu(ext2_inode->i_blocks);
  zi->i_flags     = le32_to_cpu(ext2_inode->i_flags);
  zi->i_faddr     = le32_to_cpu(ext2_inode->i_faddr);
  zi->i_frag_no   = ext2_inode->osd2.linux2.l_i_frag;
  zi->i_frag_size = ext2_inode->osd2.linux2.l_i_fsize;
  zi->i_file_acl  = le32_to_cpu(ext2_inode->i_file_acl);
  zi->i_dir_acl = 0;
  if (S_ISREG(inode->i_mode)) {
    inode->i_size |= ((__u64)le32_to_cpu(ext2_inode->i_dir_acl)) << 32;
  } else {
    zi->i_dir_acl = le32_to_cpu(ext2_inode->i_dir_acl);
  }
  inode->i_generation = le32_to_cpu(ext2_inode->i_generation);
  zi->i_dtime            = 0;
  zi->i_state            = 0;
  zi->i_block_group      = (ino - 1) / ZARUFS_SB(sb)->s_inodes_per_group;
  zi->i_dir_start_lookup = 0;

  for (i = 0; i < ZARUFS_NR_BLOCKS; i++) {
    zi->i_data[i] = ext2_inode->i_block[i];
  }

  if (S_ISREG(inode->i_mode)) {
  } else if (S_ISDIR(inode->i_mode)) {
    DBGPRINT("[ZARUFS] get directory inode!\n");
    inode->i_fop = &zarufs_dir_operations;
    inode->i_op  = &zarufs_dir_inode_operations;
  } else if (S_ISLNK(inode->i_mode)) {
  } else {
  }

  brelse(bh);
  zarufs_set_vfs_inode_flags(inode);
  /* debug_print_zarufs_inode_info(zi); */
  /* debug_print_vfs_inode(inode); */
  return (inode);
}

int
zarufs_write_inode(struct inode *inode, struct writeback_control *wbc) {
  DBGPRINT("[ZARUFS] %s called!\n", __func__);
  return (__zarufs_write_inode(inode, wbc->sync_mode == WB_SYNC_ALL));
}

void
zarufs_set_zarufs_inode_flags(struct zarufs_inode_info *zi) {
  unsigned int flags;

  flags = zi->vfs_inode.i_flags;
  zi->i_flags &= ~(EXT2_SYNC_FL | EXT2_APPEND_FL | EXT2_IMMUTABLE_FL | EXT2_NOATIME_FL | EXT2_DIRSYNC_FL);
  if (flags & S_SYNC) {
    zi->i_flags |= EXT2_SYNC_FL;
  }
  if (flags & S_APPEND) {
    zi->i_flags |= EXT2_APPEND_FL;
  }
  if (flags & S_IMMUTABLE) {
    zi->i_flags |= EXT2_IMMUTABLE_FL;
  }
  if (flags & S_NOATIME) {
    zi->i_flags |= EXT2_NOATIME_FL;
  }
  if (flags & S_DIRSYNC) {
    zi->i_flags |= EXT2_DIRSYNC_FL;
  }
}

void
zarufs_set_vfs_inode_flags(struct inode *inode) {
  unsigned int flags;
  flags = ZARUFS_I(inode)->i_flags;
  inode->i_flags &= ~(S_SYNC | S_APPEND | S_IMMUTABLE | S_NOATIME | S_DIRSYNC );
  if (flags & EXT2_SYNC_FL) {
    inode->i_flags |= S_SYNC;
  }
  if (flags & EXT2_APPEND_FL) {
    inode->i_flags |= S_APPEND;
  }
  if (flags & EXT2_IMMUTABLE_FL) {
    inode->i_flags |= S_IMMUTABLE;
  }
  if (flags & EXT2_NOATIME_FL) {
    inode->i_flags |= S_NOATIME;
  }
  if (flags & EXT2_DIRSYNC_FL) {
    inode->i_flags |= S_DIRSYNC;
  }
}

static int
zarufs_read_page(struct file *filp, struct page *page) {
  DBGPRINT("[ZARUFS] read page\n");
  return(mpage_readpage(page, zarufs_get_block));
}

static int
zarufs_read_pages(struct file *filp,
                  struct address_space *mapping,
                  struct list_head *pages,
                  unsigned nr_pages) {
  DBGPRINT("[ZARUFS] read page[s]\n");
  return (mpage_readpages(mapping, pages, nr_pages, zarufs_get_block));
}


int
zarufs_get_block(struct inode *inode,
                 sector_t iblock,
                 struct buffer_head *bh_result,
                 int create) {
  unsigned long maxblocks;
  int           ret;

  if (!inode) {
    ZARUFS_ERROR("[ZARUFS] %s:inode is NULL!\n", __func__);
    return(0);
  }

  DBGPRINT("[ZARUFS] %s:inode info\n", __func__);
  DBGPRINT("[ZARUFS] ino=%lu\n", inode->i_ino);
  maxblocks = bh_result->b_size >> inode->i_blkbits;
  ret = zarufs_get_blocks(inode, iblock, maxblocks, bh_result, create);
  if (0 < ret) {
    bh_result->b_size = (ret << inode->i_blkbits);
    ret = 0;
  }
  return (ret);
}

static int
zarufs_block_to_path(struct inode *inode,
                     unsigned long i_block,
                     int offsets[4],
                     int *boundary) {
  unsigned int  ind_blk_entries;
  unsigned long direct_blocks;
  unsigned long first_ind_blocks;
  unsigned long second_ind_blocks;
  unsigned long third_ind_blocks;
  unsigned long final;
  int           depth;

  ind_blk_entries = inode->i_sb->s_blocksize / sizeof(__le32);
  direct_blocks   = ZARUFS_NDIR_BLOCKS;
  final = 0;
  depth = 0;
  /* direct block. */
  if (i_block < direct_blocks) {
    offsets[depth++] = i_block;
    final            = direct_blocks;
    DBGPRINT("[ZARUFS] get_blocks:\n");
    DBGPRINT("[ZARUFS] i_block=%lu, depth=%d\n", i_block, depth);
    goto calc_boundary;
  }

  /* 1st. indirect blocks. */
  first_ind_blocks = direct_blocks + ind_blk_entries;
  if (i_block < first_ind_blocks) {
    i_block          = i_block - direct_blocks;
    offsets[depth++] = ZARUFS_IND_BLOCK;
    offsets[depth++] = i_block;
    final            = ind_blk_entries;
    DBGPRINT("[ZARUFS] get_blocks:\n");
    DBGPRINT("[ZARUFS] i_block=%lu, depth=%d\n", i_block, depth);
    goto calc_boundary;
  }

  /* 2nd. indirect blocks. */
  second_ind_blocks = first_ind_blocks + ind_blk_entries * ind_blk_entries;
  if (i_block < second_ind_blocks) {
    i_block          = i_block - first_ind_blocks;
    offsets[depth++] = ZARUFS_2IND_BLOCK;
    offsets[depth++] = i_block / ind_blk_entries;
    offsets[depth++] = i_block & (ind_blk_entries - 1);
    final            = ind_blk_entries;
    DBGPRINT("[ZARUFS] get_blocks:\n");
    DBGPRINT("[ZARUFS] i_block=%lu, depth=%d\n", i_block, depth);
    goto calc_boundary;
  }
  
  /* 3rd. indirect blocks. */
  third_ind_blocks = second_ind_blocks
    + ( ind_blk_entries * ind_blk_entries * ind_blk_entries );
    
  if (i_block < third_ind_blocks) {
    unsigned long   i_index;
    i_block = i_block - second_ind_blocks;
    i_index = ( i_block / ( ind_blk_entries * ind_blk_entries ) )
      & ( ind_blk_entries - 1 );
    offsets[ depth++ ]  = ZARUFS_3IND_BLOCK;
    offsets[ depth++ ]  = i_index;
    offsets[ depth++ ]  = ( i_block / ind_blk_entries )
      & ( ind_blk_entries - 1 );
    offsets[ depth++ ]  = i_block
      & ( ind_blk_entries - 1 );
    final           = ind_blk_entries;
    DBGPRINT("[ZARUFS] get_blocks:\n");
    DBGPRINT("[ZARUFS] i_block=%lu, depth=%d\n", i_block, depth);
  } else {
    /* too big block number. */
    ZARUFS_ERROR("[ZARUFS] %s: Too big block number!!!\n", __func__);
    final = 0;
  }
  
 calc_boundary:
  if (boundary) {
    *boundary = final - 1 - (i_block & (ind_blk_entries - 1));
  }
  return (depth);
}

static inline void
add_chain(indirect *p, struct buffer_head *bh, __le32 *v) {
  p->p   = v;
  p->key = *v;
  p->bh  = bh;
}


static indirect*
zarufs_get_branch(struct inode *inode,
                  int          depth,
                  int          *offsets,
                  indirect     chain[4],
                  int          *err) {
  struct zarufs_inode_info *zi;
  struct buffer_head       *bh;
  indirect                 *p;

  zi   = ZARUFS_I(inode);
  p    = chain;
  *err = 0;

  add_chain(chain, NULL, ZARUFS_I(inode)->i_data + *offsets);
  if (!p->key) {
    goto no_block;
  }
  while (--depth) {
    if (!(bh = sb_bread(inode->i_sb, le32_to_cpu(p->key)))) {
      *err = -EIO;
      goto no_block;
    }
    read_lock(&ZARUFS_I(inode)->i_meta_lock);
    if (!verify_indirect_chain(chain, p)) {
      goto truncated;
    }

    add_chain(++p, bh, (__le32*) bh->b_data + *(++offsets));
    read_unlock(&ZARUFS_I(inode)->i_meta_lock);
    if (!p->key) {
      goto no_block;
    }
  }
  return (NULL);

 truncated:
  read_unlock(&(zi->i_meta_lock));
  brelse(bh);
  *err = -EAGAIN;

 no_block:
  return(p);
}

static int
zarufs_get_blocks(struct inode *inode,
                  sector_t iblock,
                  unsigned long maxblocks,
                  struct buffer_head *bh_result,
                  int create) {
  struct zarufs_inode_info *zi;
  indirect                 chain[4];
  indirect                 *partial;
  int                      offsets[4];
  int                      blocks_to_boundary;
  int                      count;
  int                      depth;
  int                      err;
  int                      indirect_blks;
  unsigned long            goal;

  /* translate block number to its reference path. */
  if (!(depth = zarufs_block_to_path(inode,
                                     iblock,
                                     offsets,
                                     &blocks_to_boundary))) {
    ZARUFS_ERROR("[ZARUFS] %s: cannot translate block number\n", __func__);
    ZARUFS_ERROR("[ZARUFS] iblock=%lu\n", iblock);
    return(-EIO);
  }

  /* find a block. */
  count = 0;
  partial = zarufs_get_branch(inode, depth, offsets, chain, &err);
  if (!partial) {
    unsigned long first_block;

    first_block = le32_to_cpu(chain[depth - 1].key);
    clear_buffer_new(bh_result);
    count++;
    while ((count < maxblocks) && (count <= blocks_to_boundary)) {
      unsigned long cur_blk;

      /* verify indirects chain. */
      if (!verify_indirect_chain(chain, chain + depth - 1)) {
        /* indirect block might be removed by truncate while */
        /* reading it. forget and go to reread. */
        err   = -EAGAIN;
        count = 0;
        break;
      }

      cur_blk = le32_to_cpu(*(chain[depth - 1].p + count));
      if (cur_blk == first_block + count) {
        count++;
      } else {
        break;
      }
    }

    if (err != -EAGAIN) {
      goto found;
    }
  }

  if (!create || (err == -EIO)) {
    goto cleanup;
  }

  zi = ZARUFS_I(inode);
  mutex_lock(&zi->truncate_mutex);
  if ((err == -EAGAIN) || !verify_indirect_chain(chain, partial)) {
    while (chain < partial) {
      brelse(partial->bh);
      partial--;
    }
    partial = zarufs_get_branch(inode, depth, offsets, chain, &err);
    if (!partial) {
      count++;
      mutex_unlock(&zi->truncate_mutex);
      if (err) {
        goto cleanup;
      }
      clear_buffer_new(bh_result);
      goto found;
    }
  }
  
  /* now allocate block. */
  goal = find_goal(inode, iblock, partial);

  /* the number of blocks need to allocte for [d,t] indrect blocks. */
  indirect_blks = (chain + depth) - partial - 1;

  /* next, lookup the indirect map to count the total number of direct blocks */
  /* to allocate for this branch. */
  count = maxblocks;
  err = alloc_branch(inode,
                     indirect_blks,
                     &count,
                     goal,
                     offsets + (partial - chain),
                     partial);
  if (err) {
    DBGPRINT("[ZARUFS] %s: cannot allocate blocks in alloc_branch.\n",
             __func__);
    mutex_unlock(&zi->truncate_mutex);
    goto cleanup;
  }

  splice_branch(inode, iblock, partial, indirect_blks, count);
  mutex_unlock(&zi->truncate_mutex);
  set_buffer_new(bh_result);

 found:
  map_bh(bh_result, inode->i_sb, le32_to_cpu(chain[depth - 1].key));
  err = count;

 cleanup:
  DBGPRINT("[ZARUFS] %s: cleanup\n", __func__);
  while (chain < partial) {
    brelse(partial->bh);
    partial--;
  }
  return(err);
}

static int
zarufs_write_page(struct page *page, struct writeback_control *wbc) {
  DBGPRINT("[ZARUFS] write page.\n");
  return(block_write_full_page(page, zarufs_get_block, wbc));
}

static int
zarufs_write_pages(struct address_space *mapping,
                   struct writeback_control *wbc) {
  DBGPRINT("[ZARUFS] write page[s].\n");
  DBGPRINT("ino=%lu\n", mapping->host->i_ino);
  return(mpage_writepages(mapping, wbc, zarufs_get_block));
}

static int
zarufs_write_begin(struct file          *file,
                   struct address_space *mapping,
                   loff_t               pos,
                   unsigned             len,
                   unsigned             flags,
                   struct page          **pagep,
                   void                 **fsdata) {
  int ret = 0;
  DBGPRINT("[ZARUFS] write begin.\n");
  ret = block_write_begin(mapping, pos, len, flags, pagep, zarufs_get_block);
  if (ret < 0) {
    /* zarufs_write_failed(mapping, pos + len); */
  }
  return(ret);
}

static int
zarufs_write_end(struct file          *file,
                 struct address_space *mapping,
                 loff_t               pos,
                 unsigned             len,
                 unsigned             copied,
                 struct page          *pagep,
                 void                 *fsdata) {
  int ret;

  DBGPRINT("[ZARUFS] write end.\n");
  ret = generic_write_end(file, mapping, pos, len, copied, pagep, fsdata);
  if (ret < len) {
    /* zarufs_write_failed(mapping, pos + len); */
  }
  return (ret);
}

static inline unsigned long
find_goal(struct inode *inode, long block, indirect *partial) {
  return (find_near(inode, partial));
}

static inline unsigned long
find_near(struct inode *inode, indirect *ind) {
  struct zarufs_inode_info  *zi;
  struct zarufs_sb_info     *zsb;
  __le32                    *start;
  __le32                    *cur;
  unsigned long             bg_start;
  unsigned long             color;

  zi = ZARUFS_I(inode);
  if (ind->bh) {
    start = (__le32*) ind->bh->b_data;
  } else {
    start = zi->i_data;
  }

  /* try to find previous block. */
  for (cur = ind->p - 1; start <= cur; cur--) {
    if (*cur) {
      return (le32_to_cpu(*cur));
    }
  }

  /* no such thing, so let's try location of indirect block. */
  if (ind->bh) {
    return (ind->bh->b_blocknr);
  }

  /* it is going to be referred from inode itself? then, just put it into */
  /* the same cylinder group. */
  zsb      = ZARUFS_SB(inode->i_sb);
  bg_start = zarufs_get_first_block_num(inode->i_sb, zi->i_block_group);
  color    = (current->pid % 16) * (zsb->s_blocks_per_group / 16);
  return(bg_start + color);
}

static int
alloc_branch(struct inode *inode,
             int indirect_blks,
             int *blks,
             unsigned long goal,
             int *offsets,
             indirect *branch) {
  int blocksize;
  int i;
  int ind_num;
  int err;

  struct buffer_head *bh;
  int                num;
  unsigned long      new_blocks[4];
  unsigned long      count;

  new_blocks[0] = zarufs_new_blocks(inode, goal, &count, &err);
  if (err) {
    return (err);
  }

  num = count;
  branch[0].key = cpu_to_le32(new_blocks[0]);
  blocksize     = inode->i_sb->s_blocksize;

  /* allocate metadata blocsk and data blocks. */
  for (ind_num = 1; ind_num < indirect_blks; ind_num++) {
    new_blocks[ind_num] = zarufs_new_blocks(inode, goal, &count, &err);
    if (err) {
      err = -ENOMEM;
      goto failed_new_blocks;
    }
    num++;
    /* get buffer head for parent block, zero it out and set the pointer */
    /* to the new one, then send parent to disc. */
    bh = sb_getblk(inode->i_sb, new_blocks[ind_num - 1]);
    if (unlikely(!bh)) {
      err = -ENOMEM;
      goto failed;
    }
    branch[ind_num].bh = bh;
    lock_buffer(bh);
    memset(bh->b_data, 0, blocksize);
    branch[ind_num].p   = (__le32*) bh->b_data + offsets[ind_num];
    branch[ind_num].key = cpu_to_le32(new_blocks[ind_num]);
    *branch[ind_num].p  = branch[ind_num].key;
    if (ind_num == indirect_blks) {
      unsigned long current_block;

      current_block = new_blocks[ind_num];
      /* end of chain, update the last new metablock of the chain to */
      /* point to the new allocated data blocks numbers. */
      for (i = 1; i < num; i++) {
        *(branch[ind_num].p + i) = cpu_to_le32(++current_block);
      }
    }

    set_buffer_uptodate(bh);
    unlock_buffer(bh);
    mark_buffer_dirty_inode(bh, inode);
    /* we use to sync bh here if IS_SYNC(inode). but we now rely upon */
    /* generic_write_sync() and b_inode_buffers. but not for directories. */
    if (S_ISDIR(inode->i_mode) && IS_DIRSYNC(inode)) {
      sync_dirty_buffer(bh);
    }
  }

  *blks = num;
  return(err);

 failed_new_blocks:
  ind_num--;

 failed:
  for (i = 1; i < ind_num; i++) {
    bforget(branch[i].bh);
  }
  for (i = 0; i < indirect_blks; i++) {
    /* as for now, i don't implement free blocks. */
    /* zarufs_free_blocks(inode, new_blocks[i]); */
  }
  /* as for now, i don't implement free blocks. */
  /* zarufs_free_blocks(inode, new_blocks[i], num); */
  return (err);
}

static void splice_branch(struct inode *inode,
                          long block,
                          indirect *where,
                          int num,
                          int blks) {
  int           i;
  unsigned long current_block;
  
  *where->p = where->key;
  /* update the host buffer_head or inode to point to more just allocated */
  /* blocks of direct blocks. */
  if ((num == 0) && (1 < blks)) {
    current_block = le32_to_cpu(where->key) + 1;
    for (i = 0; i < blks; i++) {
      *(where->p + i) = cpu_to_le32(current_block++);
    }
  }

  if (where->bh) {
    mark_buffer_dirty_inode(where->bh, inode);
  }

  inode->i_ctime = CURRENT_TIME_SEC;
  mark_inode_dirty(inode);
}

static inline int
verify_indirect_chain(indirect *from, indirect *to) {
  while ((from <= to) && (from->key == *from->p)) {
    from++;
  }
  return(to < from);
}

static int
__zarufs_write_inode(struct inode *inode, int do_sync) {
  struct zarufs_inode_info *zi;
  struct super_block       *sb;
  ino_t                    ino;
  uid_t                    uid;
  gid_t                    gid;
  struct buffer_head       *bh;
  struct ext2_inode        *ext2_inode;
  int                      err;

  DBGPRINT("[ZARUFS] %s: do_sync=%d\n", __func__, do_sync);

  sb         = inode->i_sb;
  ino        = inode->i_ino;
  ext2_inode = zarufs_get_ext2_inode(sb, ino, &bh);

  if (IS_ERR(ext2_inode)) {
    return (-EIO);
  }

  zi  = ZARUFS_I(inode);
  uid = i_uid_read(inode);
  gid = i_gid_read(inode);
  err = 0;

  /* for fields not tracking in the in-memory inode, */
  /* initialize them to zero for new inodes. */
  if (zi->i_state & EXT2_STATE_NEW) {
    memset(ext2_inode, 0x00, ZARUFS_SB(sb)->s_inode_size);
  }

  zarufs_set_zarufs_inode_flags(zi);

  ext2_inode->i_mode = cpu_to_le16(inode->i_mode);

  if (!(ZARUFS_SB(sb)->s_mount_opt & EXT2_MOUNT_NO_UID32)) {
    /* 32bit uid/gid */
    ext2_inode->i_uid = cpu_to_le16(low_16_bits(uid));
    ext2_inode->i_gid = cpu_to_le16(low_16_bits(gid));
    /* old inodes get re-used with the upper 16 bits of the uid/gid intact. */
    if (!zi->i_dtime) {
      ext2_inode->osd2.linux2.l_i_uid_high = cpu_to_le16(high_16_bits(uid));
      ext2_inode->osd2.linux2.l_i_gid_high = cpu_to_le16(high_16_bits(gid));
    } else {
      ext2_inode->osd2.linux2.l_i_uid_high = 0;
      ext2_inode->osd2.linux2.l_i_gid_high = 0;
    }
  } else {
    /* 16bit uid/gid */
    ext2_inode->i_uid = cpu_to_le16(fs_high2lowuid(uid));
    ext2_inode->i_gid = cpu_to_le16(fs_high2lowuid(gid));
    ext2_inode->osd2.linux2.l_i_uid_high = 0;
    ext2_inode->osd2.linux2.l_i_gid_high = 0;
  }

  ext2_inode->i_links_count = cpu_to_le16(inode->i_nlink);
  ext2_inode->i_size        = cpu_to_le32(inode->i_size);
  ext2_inode->i_atime       = cpu_to_le32(inode->i_atime.tv_sec);
  ext2_inode->i_ctime       = cpu_to_le32(inode->i_ctime.tv_sec);
  ext2_inode->i_mtime       = cpu_to_le32(inode->i_mtime.tv_sec);

  ext2_inode->i_blocks      = cpu_to_le32(inode->i_blocks);
  ext2_inode->i_dtime       = cpu_to_le32(zi->i_dtime);
  ext2_inode->i_flags       = cpu_to_le32(zi->i_flags);
  ext2_inode->i_faddr       = cpu_to_le32(zi->i_faddr);
  ext2_inode->i_file_acl    = cpu_to_le32(zi->i_file_acl);

  ext2_inode->osd2.linux2.l_i_frag  = zi->i_frag_no;
  ext2_inode->osd2.linux2.l_i_fsize = zi->i_frag_size;

  if (!S_ISREG(inode->i_mode)) {
    ext2_inode->i_dir_acl = cpu_to_le32(zi->i_dir_acl);
  } else {
    ext2_inode->i_dir_acl = cpu_to_le32(inode->i_size >> 32);
  }

  if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode)) {
    /* do not implement character/block device. */
  } else {
    int n;
    for (n = 0; n < ZARUFS_NR_BLOCKS; n++) {
      ext2_inode->i_block[n] = zi->i_data[n];
    }
  }

  mark_buffer_dirty(bh);
  if (do_sync) {
    sync_dirty_buffer(bh);
    if (buffer_req(bh) && !buffer_uptodate(bh)) {
      ZARUFS_ERROR("[ZARUFS] %s: io error syncing inode[%s:%08lx]\n",
                   __func__, sb->s_id, (unsigned long) ino);
      err = -EIO;
    }
  }

  zi->i_state &= ~EXT2_STATE_NEW;
  brelse(bh);

  return(err);
}
