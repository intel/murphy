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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>

#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/log.h>

typedef struct {
    mrp_list_hook_t  hook;
    char            *name;
    mrp_logger_t     logger;
    void            *data;
    int              builtin;
} log_target_t;

static log_target_t stderr_target;
static log_target_t stdout_target;
static log_target_t syslog_target;
static log_target_t file_target;

static MRP_LIST_HOOK(log_targets);
static int           log_mask   = MRP_LOG_MASK_ERROR;
static log_target_t *log_target = NULL;


mrp_log_mask_t mrp_log_parse_levels(const char *levels)
{
    const char *p;
    mrp_log_mask_t mask;

    mask = 0;

    if (levels == NULL) {
        if (mask == 0)
            mask = 1;
        else {
            mask <<= 1;
            mask  |= 1;
        }
    }
    else {
        p = levels;
        while (p && *p) {
#           define MATCHES(s, l) (!strcmp(s, l) ||                      \
                                  !strncmp(s, l",", sizeof(l",") - 1))

            if (MATCHES(p, "info"))
                mask |= MRP_LOG_MASK_INFO;
            else if (MATCHES(p, "error"))
                mask |= MRP_LOG_MASK_ERROR;
            else if (MATCHES(p, "warning"))
                mask |= MRP_LOG_MASK_WARNING;
            else if (MATCHES(p, "none") || MATCHES(p, "off"))
                mask = 0;
            else
                return -1;

            if ((p = strchr(p, ',')) != NULL)
                p += 1;

#           undef MATCHES
        }
    }

    return mask;
}


const char *mrp_log_parse_target(const char *target)
{
    return target;
}


const char *mrp_log_dump_mask(mrp_log_mask_t mask, char *buf, size_t size)
{
    char *p, *t;
    int   n, l;

    if (!mask)
        return "none";

    p = buf;
    l = size;

    t  = "";
    *p = '\0';

    if (mask & MRP_LOG_MASK_INFO) {
        n  = snprintf(p, l, "info");
        p += n;
        l -= n;
        t = ",";
    }
    if (mask & MRP_LOG_MASK_WARNING) {
        n  = snprintf(p, l, "%swarning", t);
        p += n;
        l -= n;
        t = ",";
    }
    if (mask & MRP_LOG_MASK_ERROR) {
        n  = snprintf(p, l, "%serror", t);
        p += n;
        l -= n;
        t = ",";
    }

    return buf;
}


mrp_log_mask_t mrp_log_enable(mrp_log_mask_t enabled)
{
    mrp_log_mask_t old_mask = log_mask;

    log_mask |= enabled;

    return old_mask;
}


mrp_log_mask_t mrp_log_disable(mrp_log_mask_t disabled)
{
    mrp_log_mask_t old_mask = log_mask;

    log_mask &= ~disabled;

    return old_mask;
}


mrp_log_mask_t mrp_log_set_mask(mrp_log_mask_t enabled)
{
    mrp_log_mask_t old_mask = log_mask;

    log_mask = enabled;

    return old_mask;
}


static log_target_t *find_target(const char *name)
{
    log_target_t    *t;
    mrp_list_hook_t *p, *n;

    mrp_list_foreach(&log_targets, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);

        if (t->name == name || !strcmp(t->name, name))
            return t;
    }

    return NULL;
}


int mrp_log_set_target(const char *name)
{
    log_target_t *target;
    const char   *path;

    if (!strncmp(name, "file:", 5)) {
        path = name + 5;
        name = "file";
    }
    else
        path = NULL;

    target = find_target(name);

    if (target == NULL || (target == &file_target && path == NULL))
        return FALSE;

    /* close files opened by us, if any */
    if (log_target == &file_target) {
        if (file_target.data != NULL) {
            fclose(file_target.data);
            file_target.data = NULL;
        }
    }

    log_target = target;

    /* open any new files if we have to */
    if (target == &file_target) {
        target->data = fopen(path, "a");

        if (target->data == NULL) {
            log_target = &syslog_target;

            return FALSE;
        }
    }

    return TRUE;
}


const char *mrp_log_get_target(void)
{
    return log_target->name;
}


int mrp_log_get_targets(const char **targets, size_t size)
{
    mrp_list_hook_t *p, *n;
    log_target_t    *t;
    int cnt;

    cnt = 0;
    mrp_list_foreach(&log_targets, p, n) {
        if (cnt == (int)size)
            break;

        t = mrp_list_entry(p, typeof(*t), hook);
        targets[cnt++] = t->name;
    }

    return cnt;
}


int mrp_log_register_target(const char *name, mrp_logger_t logger, void *data)
{
    log_target_t *target;

    if (find_target(name) != NULL)
        return FALSE;

    target = mrp_allocz(sizeof(*target));

    mrp_list_init(&target->hook);
    target->name   = mrp_strdup(name);
    target->logger = logger;
    target->data   = data;

    if (target->name != NULL) {
        mrp_list_append(&log_targets, &target->hook);

        return TRUE;
    }
    else {
        mrp_free(target);

        return FALSE;
    }
}


int mrp_log_unregister_target(const char *name)
{
    log_target_t *target;

    target = find_target(name);

    if (target == NULL || target->builtin)
        return FALSE;

    if (log_target == target)
        log_target = &stderr_target;

    mrp_list_delete(&target->hook);
    mrp_free(target->name);
    mrp_free(target);

    return TRUE;
}


static void log_msgv(void *data, mrp_log_level_t level, const char *file,
                     int line, const char *func, const char *format,
                     va_list ap)
{
    FILE       *fp = data;
    int         lvl;
    const char *prefix;
    char        prfx[2*1024];

    if (!(log_mask & (1 << level)))
        return;

    MRP_UNUSED(file);
    MRP_UNUSED(line);

    switch (level) {
    case MRP_LOG_ERROR:   lvl = LOG_ERR;     prefix = "E: "; break;
    case MRP_LOG_WARNING: lvl = LOG_WARNING; prefix = "W: "; break;
    case MRP_LOG_INFO:    lvl = LOG_INFO;    prefix = "I: "; break;
    case MRP_LOG_DEBUG:   lvl = LOG_INFO;
        snprintf(prfx, sizeof(prfx) - 1, "D: [%s] ", func);
        prfx[sizeof(prfx)-1] = '\0';
        prefix = prfx;
        break;
    default:
        return;
    }

    if (fp == NULL)
        vsyslog(lvl, format, ap);
    else {
        fputs(prefix, fp);
        vfprintf(fp, format, ap); fputs("\n", fp);
        fflush(fp);
    }
}


void mrp_log_msgv(mrp_log_level_t level, const char *file,
                  int line, const char *func, const char *format,
                  va_list ap)
{
    static int    busy   = 0;
    mrp_logger_t  logger = log_target->logger;
    void         *data   = log_target->data;

    if (MRP_UNLIKELY(busy != 0))
        return;

    if (!(log_mask & (1 << level)))
        return;

    busy++;
    logger(data, level, file, line, func, format, ap);
    busy--;
}


void mrp_log_msg(mrp_log_level_t level, const char *file,
                 int line, const char *func, const char *format, ...)
{
    va_list ap;

    if (!(log_mask & (1 << level)))
        return;

    va_start(ap, format);
    mrp_log_msgv(level, file, line, func, format, ap);
    va_end(ap);
}


/*
 * workaround for not being able to initialize log_fp to stderr
 */

static __attribute__((constructor)) void set_default_logging(void)
{
    mrp_list_init(&stderr_target.hook);
    stderr_target.name    = "stderr";
    stderr_target.logger  = log_msgv;
    stderr_target.data    = stderr;
    stderr_target.builtin = TRUE;

    mrp_list_init(&stdout_target.hook);
    stdout_target.name    = "stdout";
    stdout_target.logger  = log_msgv;
    stdout_target.data    = stdout;
    stdout_target.builtin = TRUE;

    mrp_list_init(&syslog_target.hook);
    syslog_target.name    = "syslog";
    syslog_target.logger  = log_msgv;
    syslog_target.data    = NULL;
    syslog_target.builtin = TRUE;

    mrp_list_init(&file_target.hook);
    file_target.name    = "file";
    file_target.logger  = log_msgv;
    file_target.data    = NULL;
    file_target.builtin = TRUE;

    mrp_list_prepend(&log_targets, &file_target.hook);
    mrp_list_prepend(&log_targets, &syslog_target.hook);
    mrp_list_prepend(&log_targets, &stderr_target.hook);
    mrp_list_prepend(&log_targets, &stdout_target.hook);

    log_target = &stderr_target;
}
