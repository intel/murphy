/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MURPHY_LOG_H__
#define __MURPHY_LOG_H__

/** \file
 * Logging functions and macros.
 */

#include <stdarg.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>

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

#define MRP_LOG_MASK(level) (1 << ((level)-1))   /**< mask of level */
#define MRP_LOG_UPTO(level) ((1 << (level+1))-1) /**< mask up to level */


/** Parse a string of comma-separated log level names to a log mask. */
mrp_log_mask_t mrp_log_parse_levels(const char *levels);

/** Write the given log mask as a string to the given buffer. */
const char *mrp_log_dump_mask(mrp_log_mask_t mask, char *buf, size_t size);

/** Clear current logging level and enable levels in mask. */
mrp_log_mask_t mrp_log_set_mask(mrp_log_mask_t mask);

/** Enable logging for levels in mask. */
mrp_log_mask_t mrp_log_enable(mrp_log_mask_t mask);

/** Disable logging for levels in mask. */
mrp_log_mask_t mrp_log_disable(mrp_log_mask_t mask);

/** Get the current logging level mask. */
#define mrp_log_get_mask() mrp_log_disable(0)

/**
 * Logging target names.
 */
#define MRP_LOG_NAME_STDOUT  "stdout"
#define MRP_LOG_NAME_STDERR  "stderr"
#define MRP_LOG_NAME_SYSLOG  "syslog"

/**
 * Logging targets.
 */
#define MRP_LOG_TO_STDOUT     "stdout"
#define MRP_LOG_TO_STDERR     "stderr"
#define MRP_LOG_TO_SYSLOG     "syslog"
#define MRP_LOG_TO_FILE(path) ((const char *)(path))


/** Parse a log target name to MRP_LOG_TO_*. */
const char *mrp_log_parse_target(const char *target);

/** Set logging target. */
int mrp_log_set_target(const char *target);

/** Get the current log target. */
const char *mrp_log_get_target(void);

/** Get all available logging targets. */
int mrp_log_get_targets(const char **targets, size_t size);

/** Log an error. */
#define mrp_log_error(fmt, args...) \
    mrp_log_msg(MRP_LOG_ERROR, __LOC__, fmt , ## args)

/** Log a warning. */
#define mrp_log_warning(fmt, args...) \
    mrp_log_msg(MRP_LOG_WARNING, __LOC__, fmt , ## args)

/** Log an informational message. */
#define mrp_log_info(fmt, args...) \
    mrp_log_msg(MRP_LOG_INFO, __LOC__, fmt , ## args)

/** Generic logging function. */
void mrp_log_msg(mrp_log_level_t level,
                 const char *file, int line, const char *func,
                 const char *format, ...) MRP_PRINTF_LIKE(5, 6);

/** Generic logging function for easy wrapping. */
void mrp_log_msgv(mrp_log_level_t level, const char *file,
                  int line, const char *func, const char *format, va_list ap);

/** Type for custom logging functions. */
typedef void (*mrp_logger_t)(void *user_data,
                             mrp_log_level_t level, const char *file,
                             int line, const char *func, const char *format,
                             va_list ap);

/** Register a new logging target. */
int mrp_log_register_target(const char *name, mrp_logger_t logger,
                            void *user_data);

/** Unregister the given logging target. */
int mrp_log_unregister_target(const char *name);

MRP_CDECL_END

#endif /* __MURPHY_LOG_H__ */
