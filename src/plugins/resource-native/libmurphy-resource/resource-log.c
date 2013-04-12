/*
 * Copyright (c) 2012, 2013 Intel Corporation
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

#include <stdarg.h>
#include <murphy/common/log.h>

#include "resource-api.h"
#include "resource-private.h"

static void default_logger(mrp_log_level_t level, const char *file,
                           int line, const char *func,
                           const char *format, va_list args)
{
    va_list ap;

    va_copy(ap, args);
    mrp_log_msgv(level, file, line, func, format, ap);
    va_end(ap);
}


static mrp_res_logger_t __res_logger = default_logger;

mrp_res_logger_t mrp_res_set_logger(mrp_res_logger_t logger)
{
    mrp_res_logger_t old = __res_logger;

    __res_logger = logger;

    return old;
}


void mrp_res_log_msg(mrp_log_level_t level, const char *file,
                     int line, const char *func, const char *format, ...)
{
    va_list ap;

    if (__res_logger != NULL) {
        va_start(ap, format);
        __res_logger(level, file, line, func, format, ap);
        va_end(ap);
    }
}
