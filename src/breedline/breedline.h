/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
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

#ifndef __BREEDLINE_H__
#define __BREEDLINE_H__

#include <breedline/macros.h>

BRL_CDECL_BEGIN

/** Default history buffer size (in number of items). */
#define BRL_DEFAULT_HISTORY 64

/** Type for opaque breedline context. */
struct brl_s;
typedef struct brl_s brl_t;

/** Create a new breedline context for the given file descriptor. */
brl_t *brl_create(int fd, const char *prompt);

/** Destroy the given context. */
void brl_destroy(brl_t *brl);

/** Set breedline prompt. */
int brl_set_prompt(brl_t *brl, const char *prompt);

/** Hide breedline prompt. */
void brl_hide_prompt(brl_t *brl);

/** Show breedline prompt. */
void brl_show_prompt(brl_t *brl);

/** Limit the size of history to the given number of entries. */
int brl_limit_history(brl_t *brl, size_t size);

/** Read a single line of input and put it to the given buffer. */
int brl_read_line(brl_t *brl, char *buf, size_t size);

/** Add an entry to history. Replaces oldest entry if history buffer is full. */
int brl_add_history(brl_t *brl, const char *entry);

/** In put delivery callback type, used when running in mainloop mode. */
typedef void (*brl_line_cb_t)(brl_t *brl, const char *line, void *user_data);

/** Breedline mainloop subset abstraction. */
typedef struct {
    void *(*add_watch)(void *ml, int fd,
                       void (*cb)(int fd, int events, void *user_data),
                       void *user_data);
    void  (*del_watch)(void *w);
} brl_mainloop_ops_t;

/** Set up the given context to be pumped by the given mainloop. */
int brl_use_mainloop(brl_t *brl, void *ml, brl_mainloop_ops_t *ops,
                     brl_line_cb_t cb, void *user_data);

/** Memory allocation operations. */
typedef struct {
    void *(*allocfn)(size_t size, const char *file, int line, const char *func);
    void *(*reallocfn)(void *ptr, size_t size, const char *file, int line,
                       const char *func);
    char *(*strdupfn)(const char *str, const char *file, int line,
                      const char *func);
    void  (*freefn)(void *ptr, const char *file, int line, const char *func);
} brl_allocator_t;

/** Override the default memory allocator. */
int brl_set_allocator(brl_allocator_t *allocator);

BRL_CDECL_END

#endif /* __BREEDLINE_H__ */
