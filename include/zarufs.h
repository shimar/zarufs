#ifndef _ZARUFS_H_
#define _ZARUFS_H_

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
  __u8   s_padding1;
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
  __le32 s_reserved[190];
};

#endif /* _ZARUFS_H_ */
