#ifndef _ZARUFS_H_
#define _ZARUFS_H_

#include <uapi/linux/magic.h>


#define ZARUFS_SUPER_MAGIC EXT2_SUPER_MAGIC /* 0xEF53 */

#define ZARUFS_LINK_MAX 32000

#define ZARUFS_NDIR_BLOCKS  (12)
#define ZARUFS_IND_BLOCK    ZARUFS_NDIR_BLOCKS
#define ZARUFS_2IND_BLOCK   (ZARUFS_IND_BLOCK + 1)
#define ZARUFS_3IND_BLOCK   (ZARUFS_2IND_BLOCK + 1)
#define ZARUFS_NR_BLOCKS    (ZARUFS_3IND_BLOCK + 1)

#define ZARUFS_EXT2_BAD_INO      1
#define ZARUFS_EXT2_ROOT_INO     2
#define ZARUFS_EXT2_BL_INO       5
#define ZARUFS_EXT2_UNDER_DIR_NO 6

/* defines for s_state. */
#define EXT2_VALID_FS (1)
#define EXT2_ERROR_FS (2)

/* defines for s_errors. */
#define EXT2_ERRORS_CONTINUE (1)
#define EXT2_ERRORS_RO       (2)
#define EXT2_ERRORS_PANIC    (3)

/* defines for s_creator_os. */
#define EXT2_OS_LINUX   (0)
#define EXT2_OS_HURD    (1)
#define EXT2_OS_MASIX   (2)
#define EXT2_OS_FREEBSD (3)
#define EXT2_OS_LITES   (4)

/* defines for s_rev_level. */
#define EXT2_GOOD_OLD_REV (0)
#define EXT2_DYNAMIC_REV  (1)

/* defines for s_def_resuid. */
#define EXT2_DEF_RESUID   (0)
/* defines for s_def_resgid. */
#define EXT2_DEF_RESGID   (0)


/* defines for s_feature_compat. */
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC (0x0001)
#define EXT2_FEATURE_COMPAT_IMAGC_INODES (0x0002)
#define EXT2_FEATURE_COMPAT_HAS_JOURNAL  (0x0004)
#define EXT2_FEATURE_COMPAT_EXT_ATTR     (0x0008)
#define EXT2_FEATURE_COMPAT_RESIZE_INO   (0x0010)
#define EXT2_FEATURE_COMPAT_DIR_INDEX    (0x0012)

#define EXT2_FEATURE_COMPAT_SUPP         ZARUFS_FEATURE_COMPAT_EXT_ATTR

/* defines for s_feature_ro_compat. */
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER (0x0001)
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE   (0x0002)
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR    (0x0004)
#define EXT2_FEATURE_RO_COMPAT_ANY          (0xFFFFFFFF)

#define EZT2_FEATURE_RO_COMPAT_SUPP (EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER | \
                                     EXT2_FEATURE_RO_COMPAT_LARGE_FILE   | \
                                     EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT2_FEATURE_RO_COMPAT_UNSUPPORTED ~EXT2_FEATURE_RO_COMPAT_SUPP

/* defines for s_feature_icompat. */
#define EXT2_FEATURE_INCOMPAT_COMPRESSION (0x0001)
#define EXT2_FEATURE_INCOMPAT_FILETYPE    (0x0002)
#define EXT2_FEATURE_INCOMPAT_RECOVER     (0x0004)
#define EXT2_FEATURE_INCOMPAT_JOURNAL_DEV (0x0008)
#define EXT2_FEATURE_INCOMPAT_META_BG     (0x0010)

#define EXT2_FEATURE_INCOMPAT_SUPP (EXT2_FEATURE_INCOMPAT_FILETYPE | \
                                    EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT2_FEATURE_INCOMPAT_UNSUPPORTED ~EXT2_FEATURE_INCOMPAT_SUPP


/* defines for s_default_mount_opts and s_mount_opts. */
#define EXT2_MOUNT_CHECK        (0x00000001)
#define EXT2_MOUNT_OLDALLOC     (0x00000002)
#define EXT2_MOUNT_GRPID        (0x00000004)
#define EXT2_MOUNT_DEBUG        (0x00000008)
#define EXT2_MOUNT_ERRORS_CONT  (0x00000010)
#define EXT2_MOUNT_ERRORS_RO    (0x00000020)
#define EXT2_MOUNT_ERRORS_PANIC (0x00000040)
#define EXT2_MOUNT_MINIX_DF     (0x00000080)
#define EXT2_MOUNT_NOBH         (0x00000100)
#define EXT2_MOUNT_NO_UID32     (0x00000200)
#define EXT2_MOUNT_XATTR_USER   (0x00000400)
#define EXT2_MOUNT_POSIX_ACL    (0x00000800)
#define EXT2_MOUNT_XIP          (0x00001000)
#define EXT2_MOUNT_USRQUOTA     (0x00002000)
#define EXT2_MOUNT_GRPQUOTA     (0x00004000)
#define EXT2_MOUNT_RESERVATION  (0x00008000)

struct zarufs_inode {
  __le16 i_mode;
  __le16 i_uid;
  __le16 i_size;
  __le16 i_atime;
  __le16 i_ctime;
  __le16 i_mtime;
  __le16 i_dtime;
  __le16 i_gid;
  __le16 i_links_count;
  __le16 i_blocks;
  __le16 i_flags;

  union {
    struct {
      __le32 l_i_reserved1;
    } linux1;

    struct {
      __le32 h_i_translator;
    } hurd1;

    struct {
      __le32 m_i_reserved1;
    } masix1;
  } osd1;

  __le32 i_block[ZARUFS_NR_BLOCKS];
  __le32 i_generation;
  __le32 i_file_acl;
  __le32 i_dir_acl;
  __le32 i_faddr;

  union {
    struct {
      __u8   l_i_frag;
      __u8   l_i_fsize;
      __u16  i_pad1;
      __le16 l_i_uid_high;
      __le16 l_i_gid_high;
      __u32  l_i_reserved2;
    } linux2;

    struct {
      __u8   hl_i_frag;
      __u8   h_i_fsize;
      __u16  h_i_mode_high;
      __le16 h_i_uid_high;
      __le16 h_i_gid_high;
      __u32  h_i_reserved2;
    } hurd2;

    struct {
      __u8   m_i_frag;
      __u8   m_i_fsize;
      __u16  m_pad1;
      __u32  m_i_reserved2[2];
    } masix2;
  } osd2;
};

struct zarufs_inode_info {
  __le32       i_data[ZARUFS_NR_BLOCKS];
  __u32        i_flags;
  __u32        i_faddr;
  __u8         i_frag_no;
  __u8         i_frag_size;
  __u16        i_state;
  __u32        i_file_acl;
  __u32        i_dir_acl;
  __u32        i_dtime;
  __u32        i_block_group;
  __u32        i_dir_start_lookup;
  struct inode vfs_inode;
};

/* i_flags */
#define ZARUFS_SECRM_FL        FS_SECRM_FL     /* secure deletion */
#define ZARUFS_UNRM_FL         FS_UNRM_FL      /* undelete */
#define ZARUFS_COMPR_FL        FS_COMPR_FL     /* compress file */
#define ZARUFS_SYNC_FL         FS_SYNC_FL      /* synchronous updates */
#define ZARUFS_IMMUTABLE_FL    FS_IMMUTABLE_FL /* immutable file */
#define ZARUFS_APPEND_FL       FS_APPEND_FL    /* write to file may only append */
#define ZARUFS_NODUMP_FL       FS_NODUMP_FL    /* do not dump file */
#define ZARUFS_NOATIME_FL      FS_NOATIME_FL   /* do not udpate atime */
/* i_flags reserved for compression usage */
#define ZARUFS_DIRTY_FL        FS_DIRTY_FL
#define ZARUFS_COMPRBLK_FL     FS_COMPRBLK_FL /* on or more compressed cluster */
#define ZARUFS_NOCOMP_FL       FS_NOCOMP_FL   /* do not compress */
#define ZARUFS_ECOMPR_FL       FS_ECOMPR_FL   /* compression error */
/* i_flags end compression flags */
#define ZARUFS_BTREE_FL        FS_BTREE_FL        /* btree format dir */
#define ZARUFS_INDEX_FL        FS_INDEX_FL        /* hash-indexed directory */
#define ZARUFS_IMAGIC_FL       FS_IMAGIC_FL       /* AFS directory */
#define ZARUFS_JOURNAL_DATA_FL FS_JOURNAL_DATA_FL /* for ext3 */
#define ZARUFS_NOTAIL_FL       FS_NOTAIL_FL       /* file tail should not be merged */
#define ZARUFS_DIRSYNC_FL      FS_DIRSYCN_FL      /* dirsync behaviour (directories only) */
#define ZARUFS_TOPDIR_FL       FS_TOPDIR_FL       /* top of directory herarchies */
#define ZARUFS_RESERVED_FL     FS_RESERVED_FL     /* reserved for ext2 lib */

struct zarufs_super_block {
  __le32 s_inodes_count;
  __le32 s_blocks_count;
  __le32 s_r_blocks_count;
  __le32 s_free_blocks_count;
  __le32 s_free_inodes_count;
  __le32 s_first_data_block;
  __le32 s_log_block_size;
  __le32 s_log_frag_size;
  __le32 s_blocks_per_group;
  __le32 s_frags_per_group;
  __le32 s_inodes_per_group;
  __le32 s_mtime;
  __le32 s_wtime;
  __le16 s_mnt_count;
  __le16 s_max_mnt_count;
  __le16 s_magic;
  __le16 s_state;
  __le16 s_errors;
  __le16 s_minor_rev_level;
  __le32 s_lastcheck;
  __le32 s_checkinterval;
  __le32 s_creator_os;
  __le16 s_rev_level;
  __le16 s_def_resuid;
  __le16 s_def_resgid;

  // dynamic revision.
  __le32 s_first_ino;
  __le16 s_inode_size;
  __le16 s_block_group_nr;
  __le32 s_feature_compat;
  __le32 s_feature_incompat;
  __le32 s_feature_ro_compat;
  __u8   s_uuid[16];
  char   s_volume_name[16];
  char   s_last_mounted[64];
  __le32 s_algorithm_usage_bitmap;

  // performance hints
  __u8   s_preallock_blocks;
  __u8   s_preallock_dir_blocks;
  __u16  s_padding1;

  // journaling support
  __u8   s_journal_uuid[16];
  __u32  s_journal_inum;
  __u32  s_journal_dev;
  __u32  s_last_orphan;

  // directory indexing support
  __u32  s_hash_seed[4];
  __u8   s_def_hash_version;
  __u8   s_reserved_char_pad;
  __u16  s_reserved_word_pad;

  // other options
  __le32 s_default_mount_opts;
  __le32 s_first_meta_bg;
  __u32  s_reserved[190];
};

struct zarufs_sb_info {
  /* buffer cache infomations. */
  struct zarufs_super_block *s_zsb;
  struct buffer_head        *s_sbh;
  struct buffer_head        **s_group_desc;

  /* disk information cache. */
  // super block.
  unsigned long  s_sb_block;
  unsigned long  s_mount_opt;
  unsigned short s_mount_state;

  // inode.
  unsigned short s_inode_size;
  unsigned int   s_first_ino;
  unsigned long  s_inodes_per_group;
  unsigned long  s_inodes_per_block;
  unsigned long  s_itb_per_group;

  // group.
  unsigned long  s_groups_count;
  unsigned long  s_blocks_per_group;
  unsigned long  s_desc_per_block; /* # of group desc per block. */
  unsigned long  s_gdb_count;      /* # of group desc blocks.    */

  // fragment.
  unsigned long  s_frag_size;
  unsigned long  s_frags_per_block;
  unsigned long  s_frags_per_group;

  // defaults.
  kuid_t         s_resuid;
  kgid_t         s_resgid;
};

struct ext2_group_desc {
  __le32 bg_block_bitmap;
  __le32 bg_inode_bitmap;
  __le32 bg_inode_table;
  __le16 bg_free_blocks_count;
  __le16 bg_free_inodes_count;
  __le16 bg_used_dirs_count;
  __le16 bg_pad;
  __le32 bg_reserved[3];
};

static inline struct zarufs_sb_info *ZARUFS_SB(struct super_block *sb) {
  return ((struct zarufs_sb_info*) sb->s_fs_info);
}

static inline unsigned long
zarufs_get_first_block_num(struct super_block *sb,
                           unsigned long group_no) {
  unsigned long first_block_num;
  first_block_num = group_no * le32_to_cpu(ZARUFS_SB(sb)->s_blocks_per_group)
    + le32_to_cpu(ZARUFS_SB(sb)->s_zsb->s_first_data_block);
  return (first_block_num);
}

#endif /* _ZARUFS_H_ */
