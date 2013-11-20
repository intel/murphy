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

#ifndef __MURPHY_ENV_H__
#define __MURPHY_ENV_H__

#include <stdint.h>
#include <stdbool.h>

/** Extract the value for the given key from the config string. */
const char *mrp_env_config_key(const char *config, const char *key);

/** Extract an int32 value for key, returning defval if not found. */
int32_t mrp_env_config_int32(const char *cfg, const char *key, int32_t defval);

/** Extract an uint32 value for key, returning defval if not found. */
uint32_t mrp_env_config_uint32(const char *cfg, const char *key,
                               uint32_t defval);

/** Extract a boolean value for key, returning defval if not found. */
bool mrp_env_config_bool(const char *config, const char *key, bool defval);

/** Extract a string value for key to buf (of size), defaulting to defval. */
int mrp_env_config_string(const char *cfg, const char *key,
                          const char *defval, char *buf, size_t size);

#endif /* __MURPHY_ENV_H__ */
