/*
 * Copyright (c) 2012 - 2013, Intel Corporation
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
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

const char *mrp_env_config_key(const char *config, const char *key)
{
    const char *beg;
    int         len;

    if (config != NULL) {
        len = strlen(key);

        beg = config;
        while (beg != NULL) {
            beg = strstr(beg, key);

            if (beg != NULL) {
                if ((beg == config || beg[-1] == ':') &&
                    (beg[len] == '=' || beg[len] == ':' || beg[len] == '\0'))
                    return (beg[len] == '=' ? beg + len + 1 : "");
                else
                    beg++;
            }
        }
    }

    return NULL;
}


int32_t mrp_env_config_int32(const char *cfg, const char *key, int32_t defval)
{
    const char *v;
    char       *end;
    int         i;

    v = mrp_env_config_key(cfg, key);

    if (v != NULL) {
        if (*v) {
            i = strtol(v, &end, 10);

            if (end && (!*end || *end == ':'))
                return i;
        }
    }

    return defval;
}


uint32_t mrp_env_config_uint32(const char *cfg, const char *key,
                               uint32_t defval)
{
    const char *v;
    char       *end;
    int         i;

    v = mrp_env_config_key(cfg, key);

    if (v != NULL) {
        if (*v) {
            i = strtol(v, &end, 10);

            if (end && (!*end || *end == ':'))
                return i;
        }
    }

    return defval;
}


int mrp_env_config_bool(const char *config, const char *key, bool defval)
{
    const char *v;

    v = mrp_env_config_key(config, key);

    if (v != NULL) {
        if (*v) {
            if ((!strncasecmp(v, "false", 5) && (v[5] == ':' || !v[5])) ||
                (!strncasecmp(v, "true" , 4) && (v[4] == ':' || !v[4])))
                return (v[0] == 't' || v[0] == 'T');
            if ((!strncasecmp(v, "no" , 2) && (v[2] == ':' || !v[2])) ||
                (!strncasecmp(v, "yes", 3) && (v[3] == ':' || !v[3])))
                return (v[0] == 'y' || v[0] == 'Y');
            if ((!strncasecmp(v, "on" , 2) && (v[2] == ':' || !v[2])) ||
                (!strncasecmp(v, "off", 3) && (v[3] == ':' || !v[3])))
                return (v[1] == 'n' || v[1] == 'N');
        }
        else if (*v == '\0')
            return !defval;              /* hmm... is this right */
    }

    return defval;
}


int mrp_env_config_string(const char *cfg, const char *key,
                          const char *defval, char *buf, size_t size)
{
    const char *v;
    char       *end;
    int         len;

    v = mrp_env_config_key(cfg, key);

    if (v == NULL)
        v = defval;

    end = strchr(v, ':');

    if (end != NULL)
        len = end - v;
    else
        len = strlen(v);

    len = snprintf(buf, size, "%*.*s", len, len, v);

    if (len >= (int)size - 1)
        buf[size - 1] = '\0';

    return len;
}
