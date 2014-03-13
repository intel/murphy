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

#ifndef __MURPHY_CONFIG_H__
#define __MURPHY_CONFIG_H__

#include <murphy/common/list.h>
#include <murphy/core/context.h>

#ifndef MRP_DEFAULT_CONFIG_DIR
#    define MRP_DEFAULT_CONFIG_DIR  SYSCONFDIR"/murphy"
#endif

#ifndef MRP_DEFAULT_CONFIG_FILE
#    define MRP_DEFAULT_CONFIG_FILE MRP_DEFAULT_CONFIG_DIR"/murphy.conf"
#endif

/*
 * command line processing
 */

/** Parse the command line and update context accordingly. */
void mrp_parse_cmdline(mrp_context_t *ctx, int argc, char **argv, char **envp);


/*
 * configuration file processing
 */

#define MRP_CFG_MAXLINE (16 * 1024)      /* input line length limit */
#define MRP_CFG_MAXARGS  64              /* command argument limit */

/* configuration keywords */
#define MRP_KEYWORD_LOAD    "load-plugin"
#define MRP_KEYWORD_TRYLOAD "try-load-plugin"
#define MRP_KEYWORD_AS      "as"
#define MRP_KEYWORD_IF      "if"
#define MRP_KEYWORD_ELSE    "else"
#define MRP_KEYWORD_END     "end"
#define MRP_KEYWORD_EXISTS  "plugin-exists"
#define MRP_KEYWORD_SETCFG  "set"
#define MRP_KEYWORD_ERROR   "error"
#define MRP_KEYWORD_WARNING "warning"
#define MRP_KEYWORD_INFO    "info"
#define MRP_START_COMMENT   '#'

/* known configuration variables for 'set' command */
#define MRP_CFGVAR_RESOLVER "resolver-ruleset"

typedef struct {
    mrp_list_hook_t actions;
} mrp_cfgfile_t;


/** Parse the given configuration file. */
mrp_cfgfile_t *mrp_parse_cfgfile(const char *path);

/** Execute the commands of the given parsed configuration file. */
int mrp_exec_cfgfile(mrp_context_t *ctx, mrp_cfgfile_t *cfg);

/** Free the given parsed configuration file. */
void mrp_free_cfgfile(mrp_cfgfile_t *cfg);

#endif /* __MURPHY_CONFIG_H__ */
