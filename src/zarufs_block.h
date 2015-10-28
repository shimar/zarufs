/* zarufs_block.h */
#ifndef _ZARUFS_BLOCK_H_
#define _ZARUFS_BLOCK_H_

struct ext2_group_desc*
zarufs_get_group_descripter(struct super_block *sb,
                            unsigned int block_group);

#endif
