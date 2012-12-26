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

#ifndef __MURPHY_CONSOLE_H__
#define __MURPHY_CONSOLE_H__

#include <murphy/common/list.h>
#include <murphy/common/msg.h>
#include <murphy/core/context.h>

#include <murphy/core/console-command.h>



/*
 * console requests
 *
 * Console request correspond to top-down event propagation in the console
 * communication stack. These requests are made by the core console to the
 * underlying actual console implementation, typically either as a result
 * of calls to the console abstraction layer, or in reponse to requests
 * (ie. input) coming from the actual console implementation.
 */

typedef struct {
    /** Deliver a buffer of data to the given console. */
    ssize_t (*write)(mrp_console_t *c, void *buf, size_t size);
    /** Console being closed, close the backend (do not release memory yet). */
    void (*close)(mrp_console_t *c);
    /** Console has been destroyed, release resources allocated by backend. */
    void (*free)(void *data);
    /** Set the prompt shown to the user at the console. */
    void (*set_prompt)(mrp_console_t *c, const char *prompt);
} mrp_console_req_t;


/*
 * console events
 *
 * Console events correspond to bottom-up event propagation in the console
 * communication stack. These callbacks are made by the console backend to
 * the core console to inform about relevant console events, such as new
 * console input or disconnect by the peer.
 */

typedef struct {
    /** New input available from console. */
    ssize_t (*input)(mrp_console_t *c, void *buf, size_t size);
    /** Peer has disconnected from the console. */
    void (*disconnected)(mrp_console_t *c, int error);
    /** Generate possible completions for the given input. */
    ssize_t (*complete)(mrp_console_t *c, void *input, size_t insize,
                        char **completions, size_t csize);
} mrp_console_evt_t;


#define MRP_CONSOLE_PUBLIC_FIELDS                                         \
    mrp_context_t         *ctx;                                           \
    mrp_console_req_t      req;                                           \
    mrp_console_evt_t      evt;                                           \
    int                  (*check_destroy)(mrp_console_t *c);              \
    FILE                  *stdout;                                        \
    FILE                  *stderr;                                        \
    void                  *backend_data;                                  \
    int                    busy;                                          \
    int                    destroyed : 1;                                 \
    int                    preserve : 1 /* the Kludge of Death, Sir Robin... */

struct mrp_console_s {
    MRP_CONSOLE_PUBLIC_FIELDS;
};


/**
 * Macro to mark a console busy while running a block of code.
 *
 * The backend needs to make sure the console is not freed while any console
 * request or event callback function is active. Similarly, the backend needs
 * to check if the console has been marked for destruction whenever an event
 * callback returns and trigger destruction if it is necessary and possible
 * (ie. the above criterium of not being active is fullfilled).
 *
 * These are the easiest to accomplish using the provided MRP_CONSOLE_BUSY
 * macro and the check_destroy callback member provided by mrp_console_t.
 *
 *     1) Use the provided MRP_CONSOLE_BUSY macro to enclose al blocks of
 *        code that invoke event callbacks. Do not do a return directly
 *        from within the enclosed call blocks, rather just set a flag
 *        within the block, check it after the block and do the return
 *        there if necessary.
 *
 *     2) Call mrp_console_t->check_destroy after any call to an console
 *        event callback. check_destroy will check for any pending destroy
 *        request and perform the actual destruction if it is both necessary
 *        and possible. If the console has been left intact, check_destroy
 *        returns FALSE. However, if the console has been destroyed and freed
 *        it returns TRUE, in which case the caller must not attempt to use
 *        or dereference the console any more.
 */

#ifndef __MRP_CONSOLE_DISABLE_CODE_CHECK__
#  define W mrp_log_error
#  define __CONSOLE_CHK_BLOCK(...) do {                                   \
        static int __checked = FALSE, __warned = FALSE;                   \
                                                                          \
        if (MRP_UNLIKELY(!__checked)) {                                   \
            __checked = TRUE;                                             \
            if (MRP_UNLIKELY(!__warned &&                                 \
                             strstr(#__VA_ARGS__, "return") != NULL)) {   \
                W("********************* WARNING *********************"); \
                W("* You seem to directly do a return from a block   *"); \
                W("* of code protected by MRP_CONSOLE_BUSY. Are      *"); \
                W("* you absolutely sure you know what you are doing *"); \
                W("* and that you are also doing it correctly ?      *"); \
                W("***************************************************"); \
                W("The suspicious code block is located at: ");           \
                W("  %s@%s:%d", __FUNCTION__, __FILE__, __LINE__);        \
                W("and it looks like this:");                             \
                W("---------------------------------------------");       \
                W("%s", #__VA_ARGS__);                                    \
                W("---------------------------------------------");       \
                W("If you understand what MRP_CONSOLE_BUSY does");        \
                W("and how, and you are sure about the correctness of");  \
                W("your code you can disable this error message by");     \
                W("#defining __MRP_CONSOLE_DISABLE_CODE_CHECK__");        \
                W("when compiling %s.", __FILE__);                        \
                __warned = TRUE;                                          \
            }                                                             \
        }                                                                 \
    } while (0)
#else
#  define __CONSOLE_CHK_BLOCK(...) do { } while (0)
#endif

#define MRP_CONSOLE_BUSY(c, ...) do {           \
       __CONSOLE_CHK_BLOCK(__VA_ARGS__);        \
        (c)->busy++;                            \
        __VA_ARGS__                             \
        (c)->busy--;                            \
    } while (0)


/** Create a new console instance. */
mrp_console_t *mrp_create_console(mrp_context_t *ctx, mrp_console_req_t *req,
                                  void *backend_data);

/** Close and mark a console for destruction. */
void mrp_destroy_console(mrp_console_t *mc);

/** Send (printf-compatible) formatted output to a console. */
void mrp_console_printf(mrp_console_t *mc, const char *fmt, ...);

/** Send (vprintf-compatible) formatted output to a console. */
void mrp_console_vprintf(mrp_console_t *mc, const char *fmt, va_list ap);

/** Set the prompt of a console. */
void mrp_set_console_prompt(mrp_console_t *mc);

#endif /* __MURPHY_CONSOLE_H__ */
