#ifndef __MURPHY_FILEUTILS_H__
#define __MURPHY_FILEUTILS_H__

#include <dirent.h>
#include <sys/types.h>


/*
 * Routines for scanning a directory for matching entries.
 */

/** Directory entry types. */
typedef enum {
    MRP_DIRENT_UNKNOWN = 0,                      /* unknown */
    MRP_DIRENT_NONE = 0x00,                      /* unknown */
    MRP_DIRENT_FIFO = 0x01,                      /* FIFO */
    MRP_DIRENT_CHR  = 0x02,                      /* character device */
    MRP_DIRENT_DIR  = 0x04,                      /* directory */
    MRP_DIRENT_BLK  = 0x08,                      /* block device */
    MRP_DIRENT_REG  = 0x10,                      /* regular file */
    MRP_DIRENT_LNK  = 0x20,                      /* symbolic link */
    MRP_DIRENT_SOCK = 0x40,                      /* socket */
    MRP_DIRENT_ANY  = 0xff,                      /* mask of all types */
} mrp_dirent_type_t;


#define MRP_PATTERN_GLOB  "glob:"                /* a globbing pattern */
#define MRP_PATTERN_REGEX "regex:"               /* a regexp pattern */


/** Directory scanning callback type. */
typedef int (*mrp_scan_dir_cb_t)(const char *entry, mrp_dirent_type_t type,
				 void *user_data);

/** Scan a directory, calling cb with all matching entries. */
int mrp_scan_dir(const char *path, const char *pattern, mrp_dirent_type_t mask,
		 mrp_scan_dir_cb_t cb, void *user_data);


#endif /* __MURPHY_FILEUTILS_H__ */
