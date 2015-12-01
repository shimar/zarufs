/* zarufs_block.h */
#ifndef _ZARUFS_BLOCK_H_
#define _ZARUFS_BLOCK_H_

struct ext2_group_desc*
zarufs_get_group_descriptor(struct super_block *sb,
                            unsigned int block_group);

int zarufs_has_bg_super(struct super_block *sb, int group);

struct buffer_head*
zarufs_get_gdesc_buffer_cache(struct super_block *sb, unsigned int block_group);

unsigned long
zarufs_count_free_blocks(struct super_block *sb);

unsigned long
zarufs_new_blocks(struct inode *inode,
                  unsigned goal,
                  unsigned long *count,
                  int *err);

#endif
