#ifndef __MURPHY_LOG_H__
#define __MURPHY_LOG_H__

/** \file
 * Logging functions and macros.
 */

#include <syslog.h>

#include <murphy/common/macros.h>

MRP_CDECL_BEGIN

#define MRP_LOG_NAME_ERROR   "error"             /**< name for error level */
#define MRP_LOG_NAME_WARNING "warning"           /**< name for warning level */
#define MRP_LOG_NAME_INFO    "info"              /**< name for info level */
#define MRP_LOG_NAME_DEBUG   "debug"             /**< name for debug level */


/**
 * Logging levels.
 */
typedef enum {
    MRP_LOG_ERROR = 0,                           /**< error log level */
    MRP_LOG_WARNING,                             /**< warning log level */
    MRP_LOG_INFO,                                /**< info log level  */
    MRP_LOG_DEBUG,                               /**< debug log level */
} mrp_log_level_t;


/**
 * Logging masks.
 */
typedef enum {
    MRP_LOG_MASK_ERROR   = 0x01,                 /**< error logging mask */
    MRP_LOG_MASK_WARNING = 0x02,                 /**< warning logging mask */
    MRP_LOG_MASK_INFO    = 0x04,                 /**< info logging mask */
    MRP_LOG_MASK_DEBUG   = 0x08,                 /**< debug logging mask */
} mrp_log_mask_t;

#define MRP_LOG_MASK(level) (1 << ((level) - 1)) /**< mask of level */
#define MRP_LOG_UPTO(level) ((1 << (level)) - 1) /**< mask up to level */

/** Parse a string of comma-separated log level names to a log mask. */
mrp_log_mask_t mrp_log_parse_levels(const char *levels);

/** Clear current logging level and enable levels in mask. */
mrp_log_mask_t mrp_log_set_mask(mrp_log_mask_t mask);

/** Enable logging for levels in mask. */
mrp_log_mask_t mrp_log_enable(mrp_log_mask_t mask);

/** Disable logging for levels in mask. */
mrp_log_mask_t mrp_log_disable(mrp_log_mask_t mask);

/**
 * Logging target names.
 */
#define MRP_LOG_NAME_STDOUT  "stdout"
#define MRP_LOG_NAME_STDERR  "stderr"
#define MRP_LOG_NAME_SYSLOG  "syslog"

/**
 * Logging targets.
 */
#define MRP_LOG_TO_STDOUT     ((const char *)1)
#define MRP_LOG_TO_STDERR     ((const char *)2)
#define MRP_LOG_TO_SYSLOG     ((const char *)3)
#define MRP_LOG_TO_FILE(path) ((const char *)(path))

/** Parse a log target name to MRP_LOG_TO_*. */
const char *mrp_log_parse_target(const char *target);

/** Set logging target. */
int mrp_log_set_target(const char *target);

/** Log an error. */
#define mrp_log_error(fmt, args...) \
    mrp_log_msg(MRP_LOG_ERROR, __LOC__, fmt , ## args)

/** Log a warning. */
#define mrp_log_warning(fmt, args...) \
    mrp_log_msg(MRP_LOG_WARNING, __LOC__, fmt , ## args)

/** Log an informational message. */
#define mrp_log_info(fmt, args...) \
    mrp_log_msg(MRP_LOG_INFO, __LOC__, fmt , ## args)

/** Log an (unclassified) debugging message. */
#define mrp_debug(fmt, args...) \
    mrp_log_msg(MRP_LOG_DEBUG, __LOC__, fmt , ## args)

/** Generic logging function. */
void mrp_log_msg(mrp_log_level_t level,
		 const char *file, int line, const char *func,
		 const char *format, ...) MRP_PRINTF_LIKE(5, 6);

MRP_CDECL_END

#endif /* __MURPHY_LOG_H__ */
