#ifndef _ZARUFS_H_
#define _ZARUFS_H_

#define ZARUFS_BLOCK_SIZE 512   /* TBD */
#define ZARUFS_BLOCK_BITS 9     /* TBD */

#define ZARUFS_TYPE_REG '0'     /* Regular File. */
#define ZARUFS_TYPE_SYM '1'     /* Symlink File. */
#define ZARUFS_TYPE_DIR '2'     /* Directoreis File. */
#define ZARUFS_GNU_LONGNAME 'L' /* file name len > 100 */

#endif /* _ZARUFS_H_ */
