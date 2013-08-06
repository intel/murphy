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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include "breedline/breedline.h"
#include "breedline/mm.h"

#ifndef TRUE
#  define FALSE 0
#  define TRUE  (!FALSE)
#endif

#define BRL_UNUSED(var) (void)var

#ifdef __GNUC__
#    define BRL_UNLIKELY(cond) __builtin_expect((cond), 0)
#else
#    define BRL_UNLIKELY(cond) __builtin_expect((cond), 0)
#endif

#define BRL_CURSOR_START "\x1b[0G"       /* move cursor to start of line */
#define BRL_CURSOR_FORW  "\x1b[%dC"      /* move cursor forward by %d */
#define BRL_ERASE_RIGHT  "\x1b[0K"       /* erase right of cursor */


/*
 * breedline modes
 */

typedef enum {
    BRL_MODE_NORMAL,                     /* normal editing mode */
    BRL_MODE_SEARCH_FORW,                /* searching forward */
    BRL_MODE_SEARCH_BACK,                /* searching backward */
} brl_mode_t;


/*
 * breedline key types
 */

typedef enum {
    BRL_TYPE_INVALID = 0x000,            /* invalid key */
    BRL_TYPE_SELF    = 0x100,            /* self-inserting (normal) key */
    BRL_TYPE_COMMAND = 0x200,            /* editing command key */
    BRL_TYPE_CSEQ    = 0x400,            /* control sequence indicator */
    BRL_TYPE_MASK    = 0x700,            /* key type mask */
} brl_type_t;

#define BRL_INPUT_TYPE(in)     ((in)  & BRL_TYPE_MASK)
#define BRL_TAG_INPUT(type, c) ((type & BRL_TYPE_MASK) | ((c) & ~BRL_TYPE_MASK))
#define BRL_INPUT_DATA(in)     ((in)  & ~BRL_TYPE_MASK)

/*
 * breedline commands
 */

typedef enum {
    BRL_CMD_INVALID = 0x00,              /* invalid input */
    BRL_CMD_SELF    = 0xff,              /* self-inserting input */
    BRL_CMD_FORWARD = 0x01,              /* cursor forward */
    BRL_CMD_BACKWARD,                    /* cursor backward */
    BRL_CMD_PREV_LINE,                   /* previous line from history */
    BRL_CMD_NEXT_LINE,                   /* next line from history */
    BRL_CMD_ERASE_BEFORE,                /* erase before cursor */
    BRL_CMD_ERASE_AT,                    /* erase at cursor */
    BRL_CMD_LINE_START,                  /* cursor to start of line */
    BRL_CMD_LINE_END,                    /* cursor to end of line */
    BRL_CMD_ERASE_REST,                  /* erase till the end of line */
    BRL_CMD_ERASE_ALL,                   /* erase the whole line */
    BRL_CMD_YANK,                        /* yank at insertion point */
    BRL_CMD_PREV_WORD,                   /* cursor to previous word boundary */
    BRL_CMD_NEXT_WORD,                   /* cursor to next word boundary */
    BRL_CMD_CANCEL,                      /* cancel special command or mode */
    BRL_CMD_ENTER,                       /* enter input */
    BRL_CMD_REDRAW,                      /* redraw input prompt */
    BRL_CMD_SEARCH_BACK,                 /* search history backward */
    BRL_CMD_SEARCH_FORW,                 /* search history forward */
} brl_cmd_t;


/*
 * breedline extended control sequences
 */

typedef struct {
    const char *seq;                     /* control sequence */
    int         len;                     /* sequence length */
    int         key;                     /* mapped to this key */
} extmap_t;


/*
 * key mapping
 */

#undef  CTRL
#define CTRL(code) ((code) & 0x1f)
#define ESC  0x1b
#define DEL  0x7f
#define BELL 0x7

#define MAP(in, out)             [(in)]            = (out)
#define MAP_RANGE(min, max, out) [(min) ... (max)] = (out)
#define CMD(cmd)                 BRL_TAG_INPUT(BRL_TYPE_COMMAND, BRL_CMD_##cmd)

static int key_map[256] = {
    MAP_RANGE(' ' , '~', BRL_TYPE_SELF    ),
    MAP(CTRL('b')      , CMD(BACKWARD)    ),
    MAP(CTRL('f')      , CMD(FORWARD)     ),
    MAP(CTRL('p')      , CMD(PREV_LINE)   ),
    MAP(CTRL('n')      , CMD(NEXT_LINE)   ),
    MAP(CTRL('d')      , CMD(ERASE_AT)    ),
    MAP(    DEL        , CMD(ERASE_BEFORE)),
    MAP(CTRL('a')      , CMD(LINE_START)  ),
    MAP(CTRL('e')      , CMD(LINE_END)    ),
    MAP(CTRL('k')      , CMD(ERASE_REST)  ),
    MAP(CTRL('u')      , CMD(ERASE_ALL)   ),
    MAP(CTRL('y')      , CMD(YANK)        ),
    MAP(CTRL('m')      , CMD(ENTER)       ),
    MAP(CTRL('l')      , CMD(REDRAW)      ),
    MAP(CTRL('r')      , CMD(SEARCH_BACK) ),
    MAP(CTRL('s')      , CMD(SEARCH_FORW) ),
    MAP(    ESC        , BRL_TYPE_CSEQ    ),
};

static extmap_t ext_map[] = {
    { "\x1b[A"   , 3, CMD(PREV_LINE)   },
    { "\x1b[B"   , 3, CMD(NEXT_LINE)   },
    { "\x1b[C"   , 3, CMD(FORWARD)     },
    { "\x1b[D"   , 3, CMD(BACKWARD)    },
    { "\x1b[F"   , 3, CMD(LINE_END)    },
    { "\x1b[H"   , 3, CMD(LINE_START)  },
    { "\x1b[1;5A", 6, BRL_TYPE_INVALID },
    { "\x1b[1;5B", 6, BRL_TYPE_INVALID },
    { "\x1b[1;5C", 6, CMD(NEXT_WORD)   },
    { "\x1b[1;5D", 6, CMD(PREV_WORD)   },
    { NULL       , 0, BRL_TYPE_INVALID }
};


/*
 * ring buffer for saving history
 */

typedef struct {
    char **entries;                      /* ring buffer entries */
    int    size;                         /* buffer size */
    int    next;                         /* buffer insertion point */
    int    srch;                         /* buffer search point */
    char  *pattern;                      /* search pattern buffer */
    int    psize;                        /* pattern buffer size */
    int    plen;                         /* actual pattern length */
} ringbuf_t;



/*
 * breedline context
 */

struct brl_s {
    int                 fd;              /* file descriptor to read */
    struct termios      term_mode;       /* original terminal settings */
    int                 term_flags;      /* terminal descriptor flags */
    int                 term_blck;       /* originally in blocking mode */
    int                 term_ncol;       /* number of columns */
    void               *ml;              /* mainloop, if any */
    brl_mainloop_ops_t *ml_ops;          /* mainloop operations, if any */
    void               *ml_w;            /* I/O watch */
    brl_line_cb_t       line_cb;         /* input callback */
    void               *user_data;       /* opaque callback data */
    char               *prompt;          /* prompt string */
    int                 hidden;          /* whether prompt is hidden */
    int                 mode;            /* current mode */
    unsigned char      *buf;             /* input buffer */
    int                 size;            /* size of the buffer */
    int                 data;            /* amount of data in buffer */
    int                 offs;            /* buffer insertion offset */
    unsigned char      *yank;            /* yank buffer */
    int                 yank_size;       /* yank buffer size */
    int                 yank_data;       /* yank buffer effective length */
    int                *map;             /* key map */
    extmap_t           *ext;             /* extended key map */
    int                 esc;             /* CS being collected */
    unsigned char       seq[8];          /* control sequence buffer */
    int                 seq_len;         /* sequence length */
    ringbuf_t           h;               /* history ring buffer */
    char               *saved;           /* input buffer saved during search */
    int                 dump;            /* whether to dump key input */
    char               *dbg_buf;         /* debug buffer */
    int                 dbg_size;        /* debug buffer size */
    int                 dbg_len;         /* debug buffer effective length */
};


static int setup_terminal(brl_t *brl);
static int cleanup_terminal(brl_t *brl);
static int terminal_size(int fd, int *nrow, int *ncol);
static int enable_rawmode(brl_t *brl);
static int restore_rawmode(brl_t *brl);
static int disable_blocking(brl_t *brl);
static int restore_blocking(brl_t *brl);

static void process_input(brl_t *brl);
static void reset_input(brl_t *brl);
static void dump_input(brl_t *brl);
static void redraw_prompt(brl_t *brl);

static int ringbuf_init(ringbuf_t *rb, int size);
static void ringbuf_purge(ringbuf_t *rb);

static void *_brl_default_alloc(size_t size, const char *file, int line,
                                const char *func);
static void *_brl_default_realloc(void *ptr, size_t size,
                                  const char *file, int line, const char *func);
static char *_brl_default_strdup(const char *str, const char *file, int line,
                                 const char *func);
static void _brl_default_free(void *ptr,
                              const char *file, int line, const char *func);



brl_t *brl_create(int fd, const char *prompt)
{
    static int  dump = -1, dbg_size = -1;
    brl_t      *brl;

    if (dump < 0) {
        const char *val = getenv("__BREEDLINE_DUMP_KEYS");
        dump = (val != NULL && (*val == 'y' || *val == 'Y'));
    }

    if (dbg_size < 0) {
        const char *val = getenv("__BREEDLINE_DEBUG");
        dbg_size = (val == NULL ? 0 : atoi(val));
    }

    brl = brl_allocz(sizeof(*brl));

    if (brl != NULL) {
        brl->fd     = fd;
        brl->map    = key_map;
        brl->ext    = ext_map;
        brl->prompt = brl_strdup(prompt ? prompt : "");

        brl->dump     = dump;
        brl->dbg_size = dbg_size;
        if (dbg_size > 0)
            brl->dbg_buf = brl_allocz(dbg_size);

        brl_limit_history(brl, BRL_DEFAULT_HISTORY);

        if (!setup_terminal(brl) && !terminal_size(fd, NULL, &brl->term_ncol))
            return brl;
        else
            brl_destroy(brl);
    }

    return NULL;
}


void brl_destroy(brl_t *brl)
{
    if (brl != NULL) {
        brl_hide_prompt(brl);
        brl_free(brl->prompt);
        brl_free(brl->buf);
        brl_free(brl->saved);
        brl_free(brl->yank);
        brl_free(brl->dbg_buf);
        ringbuf_purge(&brl->h);
        cleanup_terminal(brl);

        brl_free(brl);
    }
}


int brl_set_prompt(brl_t *brl, const char *prompt)
{
    brl_free(brl->prompt);
    brl->prompt = brl_strdup(prompt);

    return (brl->prompt != NULL || prompt == NULL);
}


void brl_hide_prompt(brl_t *brl)
{
    static int warned = 0;
    char buf[32];
    int  n, o;

    brl->hidden = TRUE;

    n = snprintf(buf, sizeof(buf), "%s%s", BRL_CURSOR_START, BRL_ERASE_RIGHT);
    o = write(brl->fd, buf, n);
    restore_rawmode(brl);

    if (BRL_UNLIKELY(o < 0 && !warned)) {           /* make gcc happy */
        fprintf(stderr, "write to fd %d failed\n", brl->fd);
        warned = 1;
    }
}


void brl_show_prompt(brl_t *brl)
{
    brl->hidden = FALSE;
    enable_rawmode(brl);
    redraw_prompt(brl);
}


static void debug(brl_t *brl, const char *fmt, ...)
{
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(brl->dbg_buf, brl->dbg_size, fmt, ap);
    va_end(ap);

    if (n > 0)
        brl->dbg_len = n < brl->dbg_size ? n : brl->dbg_size;
    else
        brl->dbg_len = 0;
}


static int ringbuf_init(ringbuf_t *rb, int size)
{
    int i;

    for (i = 0; i < rb->size; i++)
        brl_free(rb->entries[i]);

    brl_free(rb->entries);
    rb->entries = NULL;
    rb->size    = 0;
    rb->next    = 0;
    rb->srch    = 0;

    brl_free(rb->pattern);
    rb->pattern = NULL;
    rb->psize   = 0;
    rb->plen    = 0;

    rb->entries = brl_allocz(size * sizeof(rb->entries[0]));

    if (rb->entries == NULL && size != 0)
        return -1;

    rb->size = size;

    return 0;
}


static void ringbuf_purge(ringbuf_t *rb)
{
    ringbuf_init(rb, 0);
}


static char **ringbuf_entry(ringbuf_t *rb, int idx)
{
    int i;

    if (rb->entries == NULL) {
        errno = ENOSPC;
        return NULL;
    }

    if (idx >= rb->size) {
        errno = EOVERFLOW;
        return NULL;
    }

    if (idx < 0 && -idx > rb->size) {
        errno = EOVERFLOW;
        return NULL;
    }

    if (idx == 0)
        return rb->entries + rb->next;

    if (idx < 0) {
        i = rb->next + idx;

        if (i < 0)
            i += rb->size;

        return rb->entries + i;
    }
    else {
        i = rb->next - 1 + idx;

        if (i >= rb->size)
            i -= rb->size;

        return rb->entries + i;
    }
}


static int ringbuf_add(ringbuf_t *rb, const char *str)
{
    char **entry = ringbuf_entry(rb, 0);

    if (entry != NULL) {
        brl_free(*entry);
        *entry   = brl_strdup(str);
        rb->next = (rb->next + 1) % rb->size;

        return 0;
    }
    else
        return -1;
}


static void ringbuf_reset_search(ringbuf_t *rb)
{
    rb->srch = 0;
    rb->plen = 0;
    memset(rb->pattern, 0, rb->psize);
}


static char *ringbuf_search(ringbuf_t *rb, int dir, unsigned char c,
                            brl_mode_t mode, char *current)
{
    int    i;
    char **e;

    if (!c && mode == BRL_MODE_NORMAL) {
        i = rb->srch + (dir < 0 ? -1 : +1);

        if (i > 0)
            return NULL;

        if (i == 0) {
            rb->srch = 0;
            return current;
        }

        e = ringbuf_entry(rb, i);

        if (e != NULL && *e != NULL) {
            rb->srch = i;
            return *e;
        }
        else
            return NULL;
    }
    else if (mode == BRL_MODE_SEARCH_BACK) {
        int total = rb->plen + 1;
        int found = 0;

        if (c) {
            if (rb->psize == 0) {
                rb->psize = 32;
                if (!(rb->pattern = brl_allocz(rb->psize))) {
                    errno = ENOMEM;
                    return NULL;
                }
            }

            if (rb->psize < total) {
                if (rb->psize * 2 > total)
                    total = rb->psize * 2;
                if (!brl_reallocz(rb->pattern, rb->psize, total)) {
                    errno = ENOMEM;
                    return NULL;
                }
                rb->psize = total;
            }

            rb->pattern[rb->plen++] = c;

            i = rb->srch;
        }
        else {
            /* keep searching backwards */
            i = rb->srch - 1;
        }

        if (!rb->pattern) {
            errno = EINVAL;
            return NULL;
        }

        /* start searching backwards from current search point */

        do {
            e = ringbuf_entry(rb, i);

            if (e != NULL && *e != NULL) {
                /* pattern matching */
                if (strstr(*e, rb->pattern)) {
                    found = 1;
                    break;
                }
            }

            i--;
        } while (e != NULL && !found);

        if (found) {
            /* set the search position while we're in search mode */
           rb->srch = i;
           return *e;
        }

        errno = ENOENT;
        return NULL;
    }

    errno = EOPNOTSUPP;
    return NULL;
}


int brl_limit_history(brl_t *brl, size_t size)
{
    return ringbuf_init(&brl->h, size);
}


int brl_add_history(brl_t *brl, const char *str)
{
    return ringbuf_add(&brl->h, str);
}


static int _brl_process_input(brl_t *brl)
{
    if (brl->dump)
        dump_input(brl);
    else
        process_input(brl);

    return 0;
}


int brl_read_line(brl_t *brl, char *buf, size_t size)
{
    if (brl->ml == NULL && brl->ml_ops == NULL) {
        reset_input(brl);
        enable_rawmode(brl);
        brl_show_prompt(brl);
        redraw_prompt(brl);
        _brl_process_input(brl);

        if (brl->data > 0)
            snprintf(buf, size, "%s", brl->buf);
        else
            buf[0] = '\0';

        brl_hide_prompt(brl);
        restore_rawmode(brl);

        return brl->data;
    }
    else {
        errno = EINPROGRESS;
        return -1;
    }
}


static void _brl_io_cb(int fd, int events, void *user_data)
{
    brl_t *brl = (brl_t *)user_data;

    BRL_UNUSED(fd);

    if (events & POLLIN)
        _brl_process_input(brl);

    if (events & POLLHUP) {
        brl->ml_ops->del_watch(brl->ml_w);
        brl->ml_w = NULL;
    }
}


int brl_use_mainloop(brl_t *brl, void *ml, brl_mainloop_ops_t *ops,
                     brl_line_cb_t cb, void *user_data)
{
    if (brl != NULL) {
        if (brl->ml != NULL || brl->ml_ops != NULL) {
            errno = EBUSY;
            return -1;
    }

        brl->line_cb   = cb;
        brl->user_data = user_data;

        brl->ml     = ml;
        brl->ml_ops = ops;
        brl->ml_w   = brl->ml_ops->add_watch(ml, brl->fd, _brl_io_cb, brl);

        if (brl->ml_w != NULL) {
            disable_blocking(brl);
            return 0;
        }
        else
            errno = EIO;
    }
    else
        errno = EFAULT;

    return -1;
}


static int enable_rawmode(brl_t *brl)
{
    struct termios mode;

    memcpy(&mode, &brl->term_mode, sizeof(mode));

    mode.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    mode.c_oflag &= ~OPOST;
    mode.c_cflag |= CS8;
    mode.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    mode.c_cc[VMIN]  = 1;
    mode.c_cc[VTIME] = 0;

    return tcsetattr(brl->fd, TCSAFLUSH, &mode);
}


static int restore_rawmode(brl_t *brl)
{
    return tcsetattr(brl->fd, TCSAFLUSH, &brl->term_mode);
}


static int disable_blocking(brl_t *brl)
{
    int flags;

    if (brl->term_blck) {
        brl->term_flags = fcntl(brl->fd, F_GETFL);

        if (brl->term_flags != -1) {
            flags = brl->term_flags | O_NONBLOCK;

            if (fcntl(brl->fd, F_SETFL, flags) == 0) {
                brl->term_blck = FALSE;
                return 0;
            }
        }

        return -1;
    }

    return 0;
}


static int restore_blocking(brl_t *brl)
{
    if (!brl->term_blck) {
        if (fcntl(brl->fd, F_SETFL, brl->term_flags) == 0) {
            brl->term_blck = FALSE;
            return 0;
        }
        else
            return -1;
    }

    return 0;
}


static int setup_terminal(brl_t *brl)
{
    if (!isatty(brl->fd)) {
        errno = ENOTTY;
        return -1;
    }

    if (tcgetattr(brl->fd, &brl->term_mode) == -1)
        return -1;

    if (enable_rawmode(brl) != 0)
        return -1;

    brl->term_flags = fcntl(brl->fd, F_GETFL);
    brl->term_blck  = TRUE;

#if 0
    if (disable_blocking(brl) == 0)
        return 0;
    else
        return -1;
#else
    return 0;
#endif
}


static int cleanup_terminal(brl_t *brl)
{
    return (restore_rawmode(brl) | restore_blocking(brl));
}


static int terminal_size(int fd, int *nrow, int *ncol)
{
    struct winsize ws;
    int    col, row;

    if (ioctl(fd, TIOCGWINSZ, &ws) == 0) {
        row = (ws.ws_row > 0 ? ws.ws_row : 80);
        col = (ws.ws_col > 0 ? ws.ws_col : 25);

        if (nrow != NULL)
            *nrow = row;
        if (ncol != NULL)
            *ncol = col;

        return 0;
    }
    else
        return -1;
}


static void redraw_prompt(brl_t *brl)
{
    static int warned = 0;

    char *prompt, *buf, *p;
    int   plen, dlen, space, start, trunc;
    int   l, n, o;
    char  search_buf[256];

    if (brl->mode == BRL_MODE_SEARCH_BACK) {
        snprintf(search_buf, 256, "search backwards: '%s'",
                brl->h.pattern ? brl->h.pattern : "");
        prompt = search_buf;
    }
    else {
        prompt = brl->prompt ? brl->prompt : "";
    }

    plen   = strlen(prompt) + 2;            /* '> ' or '><' */

    if (brl->dbg_len > 0)
        plen += brl->dbg_len + 2;

    space  = brl->term_ncol - 1 - plen - 1; /* - 1 for potential trailing > */

    /* adjust start if the cursor would be past the right edge */
    if (brl->offs >= space)
        start = brl->offs - space;
    else
        start = 0;

    /* truncate if there's more data than fits the screen */
    dlen = brl->data - start;

    if (dlen > space) {
        dlen = space;
        trunc = TRUE;
    }
    else
        trunc = FALSE;

    l   = plen + dlen + 64;
    buf = alloca(l);
    p   = buf;

#if 0
    printf("\n\rplen = %d, dlen = %d, start = %d, buf = '%*.*s'\n\r",
           plen, dlen, start, brl->data, brl->data, brl->buf);
    printf("brl->offs = %d, effective offset = %d\n\r", brl->offs,
           plen + brl->offs - start);
#endif

    /* position cursor to beginning of line */
    n  = snprintf(p, l, "%s", BRL_CURSOR_START);
    p += n;
    l -= n;

    /* print prompt + visible portion of buffer */
    n  = snprintf(p, l, "%s%s%s%s%s%*.*s%s", prompt,
                  brl->dbg_len ? "[" : "",
                  brl->dbg_len ? brl->dbg_buf : "",
                  brl->dbg_len ? "]" : "",
                  start > 0 ? "><" : "> ",
                  dlen, dlen, brl->buf + start, trunc ? ">" : "");
    p += n;
    l -= n;

    /* erase the rest of the line (ie. make sure there's no trailing junk) */
    n  = snprintf(p, l, "%s", BRL_ERASE_RIGHT);
    p += n;
    l -= n;

    /* okay, perform the actions collected so far */
    o = write(brl->fd, buf, (p - buf));

    l = plen + dlen + 64;
    p = buf;

    if (BRL_UNLIKELY(o < 0 && !warned)) {           /* make gcc happy */
        fprintf(stderr, "write to fd %d failed\n", brl->fd);
        warned = 1;
    }

    /* re-position cursor to the current insertion offset */
    n  = snprintf(p, l, BRL_CURSOR_START""BRL_CURSOR_FORW,
                  plen + brl->offs - start);
    p += n;
    l -= n;
    o = write(brl->fd, buf, (p - buf));

    if (BRL_UNLIKELY(o < 0 && !warned)) {           /* make gcc happy */
        fprintf(stderr, "write to fd %d failed\n", brl->fd);
        warned = 1;
    }
}


/*
 * input buffer handling
 */

static void reset_input(brl_t *brl)
{
    memset(brl->buf, 0, brl->size);
    brl->data = 0;
    brl->offs = 0;
}


static int insert_input(brl_t *brl, const char *input, int len)
{
    int total;

    total = brl->data + len + 1;

    if (brl->size < total) {
        if (brl->size * 2 > total)
            total = brl->size * 2;
        if (!brl_reallocz(brl->buf, brl->size, total))
            return -1;
        brl->size = total;
    }

    if (brl->offs < brl->data) {
        memmove(brl->buf + brl->offs + len, brl->buf + brl->offs,
                brl->data - brl->offs);
        memcpy(brl->buf + brl->offs, input, len);
    }
    else
        memcpy(brl->buf + brl->offs, input, len);

    brl->data += len;
    brl->offs += len;
    brl->buf[brl->data] = '\0';

    return 0;
}


static int erase_input(brl_t *brl, int n)
{
    if (n < 0) {
        if (-n > brl->offs)
            n = -brl->offs;
        if (brl->offs < brl->data)
            memmove(brl->buf + brl->offs + n, brl->buf + brl->offs,
                    brl->data - brl->offs);
        brl->data += n;
        if (brl->offs > brl->data)
            brl->offs = brl->data;
    }
    else {
        if (n > brl->data - brl->offs)
            n = brl->data - brl->offs;
        memmove(brl->buf + brl->offs, brl->buf + brl->offs + n,
                brl->data - (brl->offs + n));
        brl->data -= n;
    }

    return 0;
}


static void save_input(brl_t *brl)
{
    brl_free(brl->saved);
    brl->saved = brl_strdup((char *)brl->buf);
}


static void save_yank(brl_t *brl, int start, int end)
{
    int size, len;

    if (start < 0 || start >= brl->data || end > brl->data)
        return;

    len  = end - start + 1;
    size = len + 1;

    if (brl->yank_size < size) {
        if (brl->yank_size * 2 > size)
            size = brl->yank_size * 2;
        if (!brl_reallocz(brl->yank, brl->yank_size, size))
            return;
    }

    brl->yank_size = size;

    memcpy(brl->yank, brl->buf + start, len);
    brl->yank[len] = '\0';
    brl->yank_data = len - 1;
}


static void restore_input(brl_t *brl)
{
    reset_input(brl);

    if (brl->saved != NULL) {
        insert_input(brl, brl->saved, strlen(brl->saved));
        brl_free(brl->saved);
        brl->saved = NULL;
    }
}


static int input_delimiter(brl_t *brl, int dir)
{
    static const char  delim[] = " ,;:.?!'\"-_/";
    char              *s, *p;

    if (brl->data == 0)
        return 0;

    if ((dir < 0 && brl->offs == 0) || (dir >= 0 && brl->offs >= brl->data))
        return 0;

    s = (char *)brl->buf + brl->offs;

    if (dir < 0) {
        p = s - 1;
        if (p > (char *)brl->buf && strchr(delim, *p) != NULL)
            p--;
        while (p >= (char *)brl->buf) {
            if (strchr(delim, *p) != NULL) {
                p   += 1;
                break;
            }
            else
                p--;
        }
        return p - s;
    }
    else {
        p = s;
        if (strchr(delim, *p) != NULL && s < (char *)brl->buf + brl->data)
            p++;
        while (p < (char *)brl->buf + brl->data) {
            if (strchr(delim, *p) != NULL)
                break;
            else
                p++;
        }
        return p - s;
    }

    return 0;
}


static void move_cursor(brl_t *brl, int n)
{
    brl->offs += n;

    if (brl->offs < 0)
        brl->offs = 0;
    if (brl->offs > brl->data)
        brl->offs = brl->data;
}


static void bell(brl_t *brl)
{
    int fd;

    if (brl->fd == fileno(stdin))
        fd = fileno(stderr);
    else
        fd = brl->fd;

    dprintf(fd, "%c", BELL);
}


/*
 * input mapping
 */

static int map_input(brl_t *brl, unsigned char c)
{
    int mapped;

    mapped = brl->map[c];

    if (mapped == BRL_TYPE_SELF)
        return BRL_TAG_INPUT(BRL_TYPE_SELF, c);
    else
        return mapped;
}


static int map_esc_sequence(brl_t *brl)
{
    BRL_UNUSED(brl);

    return BRL_TYPE_INVALID;
}


static int map_ctrl_sequence(brl_t *brl)
{
    extmap_t *e;
    int       d;

    if (brl->ext != NULL) {
        for (e = brl->ext; e->seq != NULL; e++) {
            if (e->len == brl->seq_len) {
                d = strncmp((char *)brl->seq, e->seq, e->len);

                if (d == 0)
                    return e->key;

                if (d < 0)
                    break;
            }

            if (e->len > brl->seq_len)
                break;
        }
    }

    return BRL_TYPE_INVALID;
}


/*
 * main input processing
 */

static void process_input(brl_t *brl)
{
    unsigned char c;
    int           mapped, type, in, n, diff;
    char          out, *line, *hentry;

    while((n = read(brl->fd, &c, sizeof(c))) > 0) {
        if (brl->esc) {
            if (brl->seq_len < (int)sizeof(brl->seq))
                brl->seq[brl->seq_len++] = c;

            if (brl->seq_len == 2) {
                if (c != '[') {
                    mapped = map_esc_sequence(brl);
                    brl->esc = FALSE;
                }
                else
                    continue;
            }
            else {
                if (0x40 <= c && c <= 0x7e) {
                    mapped = map_ctrl_sequence(brl);
                    brl->esc = FALSE;
                }
                else {
                    if (brl->seq_len == (int)sizeof(brl->seq)) {
                        mapped = BRL_TYPE_INVALID;
                        brl->esc = FALSE;
                    }
                    else
                        continue;
                }
            }
        }
        else
            mapped = map_input(brl, c);

        type = BRL_INPUT_TYPE(mapped);
        in   = BRL_INPUT_DATA(mapped);

        switch (type) {
        case BRL_TYPE_SELF:
            switch (brl->mode) {
                case BRL_MODE_NORMAL:
                    out = (char)(in & 0xff);
                    insert_input(brl, &out, 1);
                    redraw_prompt(brl);
                    break;
                case BRL_MODE_SEARCH_BACK:
                    out = (char)(in & 0xff);
                    hentry = ringbuf_search(&brl->h, 0, out, BRL_MODE_SEARCH_BACK, NULL);
                    if (hentry != NULL) {
                        reset_input(brl);
                        insert_input(brl, hentry, strlen(hentry));
                    }
                    else
                        bell(brl);
                    redraw_prompt(brl);
                    break;
                case BRL_MODE_SEARCH_FORW:
                    /* TODO */
                    break;
            }
            break;

        case BRL_TYPE_COMMAND:
            switch (in) {
            case BRL_CMD_PREV_LINE:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                if (brl->h.srch == 0)
                    save_input(brl);
                hentry = ringbuf_search(&brl->h, -1, 0, BRL_MODE_NORMAL, (char *)brl->saved);
                debug(brl, "s:%d,'%s'", brl->h.srch,
                      brl->saved ? brl->saved : "-");
                if (hentry != NULL) {
                    reset_input(brl);
                    insert_input(brl, hentry, strlen(hentry));
                    redraw_prompt(brl);
                }
                else
                    bell(brl);
                break;

            case BRL_CMD_NEXT_LINE:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                hentry = ringbuf_search(&brl->h, +1, 0, BRL_MODE_NORMAL, (char *)brl->saved);
                debug(brl, "s:%d,'%s'", brl->h.srch,
                      brl->saved ? brl->saved : "-");
                if (hentry != NULL) {
                    if (hentry == brl->saved)
                        restore_input(brl);
                    else {
                        reset_input(brl);
                        insert_input(brl, hentry, strlen(hentry));
                    }
                    redraw_prompt(brl);
                }
                else
                    bell(brl);
                break;

            case BRL_CMD_SEARCH_BACK:
                if (brl->mode == BRL_MODE_SEARCH_BACK) {
                    /* already in search mode, continue */
                    hentry = ringbuf_search(&brl->h, 0, 0, BRL_MODE_SEARCH_BACK, NULL);
                    if (hentry != NULL) {
                        reset_input(brl);
                        insert_input(brl, hentry, strlen(hentry));
                    }
                    else
                        bell(brl);
                }
                else {
                    if (brl->h.srch == 0)
                        save_input(brl);
                    brl->mode = BRL_MODE_SEARCH_BACK;
                }
                redraw_prompt(brl);
                break;

            case BRL_CMD_BACKWARD:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                move_cursor(brl, -1);
                redraw_prompt(brl);
                break;
            case BRL_CMD_FORWARD:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                move_cursor(brl, +1);
                redraw_prompt(brl);
                break;

            case BRL_CMD_LINE_START:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                move_cursor(brl, -brl->offs);
                redraw_prompt(brl);
                break;
            case BRL_CMD_LINE_END:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                move_cursor(brl, brl->data - brl->offs);
                redraw_prompt(brl);
                break;

            case BRL_CMD_ERASE_BEFORE:
                switch(brl->mode) {
                case BRL_MODE_NORMAL:
                    erase_input(brl, -1);
                    if (brl->offs < brl->data)
                        move_cursor(brl, -1);
                    redraw_prompt(brl);
                    break;
                case BRL_MODE_SEARCH_BACK:
                case BRL_MODE_SEARCH_FORW:
                    if (brl->h.plen > 0) {
                        brl->h.pattern[--brl->h.plen] = '\0';
                    }
                    else {
                        ringbuf_reset_search(&brl->h);
                        brl->mode = BRL_MODE_NORMAL;
                        restore_input(brl);
                    }
                    redraw_prompt(brl);
                    break;
                }
                break;
            case BRL_CMD_ERASE_AT:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                erase_input(brl, 1);
                redraw_prompt(brl);
                break;

            case BRL_CMD_ERASE_REST:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                save_yank(brl, brl->offs, brl->data);
                erase_input(brl, brl->data - brl->offs);
                redraw_prompt(brl);
                break;
            case BRL_CMD_ERASE_ALL:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                save_yank(brl, 0, brl->data);
                reset_input(brl);
                redraw_prompt(brl);
                break;
            case BRL_CMD_YANK:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                insert_input(brl, (char *)brl->yank, brl->yank_data);
                redraw_prompt(brl);
                break;

            case BRL_CMD_PREV_WORD:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                diff = input_delimiter(brl, -1);
                move_cursor(brl, diff);
                redraw_prompt(brl);
                break;

            case BRL_CMD_NEXT_WORD:
                if (brl->mode != BRL_MODE_NORMAL) {
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                }
                diff = input_delimiter(brl, +1);
                move_cursor(brl, diff);
                redraw_prompt(brl);
                break;

            case BRL_CMD_REDRAW:
                redraw_prompt(brl);
                break;

            case BRL_CMD_ENTER:
                dprintf(brl->fd, "\n\r");
                if (brl->line_cb != NULL) {
                    line = alloca(brl->data + 1);
                    strncpy(line, (char *)brl->buf, brl->data);
                    line[brl->data] = '\0';
                    reset_input(brl);
                    restore_rawmode(brl);
                    brl->line_cb(brl, line, brl->user_data);
                    enable_rawmode(brl);
                    ringbuf_reset_search(&brl->h);
                    brl->mode = BRL_MODE_NORMAL;
                    debug(brl, "");
                    redraw_prompt(brl);
                }
                else
                    return;
                break;

            default:
#if 0
                printf("editing command 0x%x\n\r", in);
#endif
                bell(brl);
            }
            break;

        case BRL_TYPE_CSEQ:
            brl->esc     = TRUE;
            brl->seq[0]  = c;
            brl->seq_len = 1;
            break;

        case BRL_TYPE_INVALID:
        default:
            bell(brl);
            break;
        }
    }
}


static void dump_input(brl_t *brl)
{
    unsigned char c, seq[64], s[4] = "  \0";
    int           i = 0;

    printf("got input:");

    while (read(brl->fd, &c, 1) == 1) {
        printf(" 0x%2.2x", c);
        seq[i++] = c;

        if (c == 0x3)
            exit(0);
    }

    printf("\n\r");
    seq[i] = '\0';

    printf("          ");
    for (i = 0; seq[i] != 0; i++) {
        printf(" %4d", seq[i]);
    }
    printf("\n\r");

    seq[i] = '\0';
    printf("          ");
    for (i = 0; seq[i] != '\0'; i++) {
        s[3] = c = seq[i];
        printf(" %s", (isprint(c) && c != '\n' && c != '\r' && c != '\t') ?
               (char *)s : (c == ESC ? "ESC" : "."));
    }
    printf("\n\r");
}


/*
 * default passthru allocator
 */

static void *_brl_default_alloc(size_t size, const char *file, int line,
                                const char *func)
{
    BRL_UNUSED(file);
    BRL_UNUSED(line);
    BRL_UNUSED(func);

    return malloc(size);
}

static void *_brl_default_realloc(void *ptr, size_t size,
                                  const char *file, int line, const char *func)
{
    BRL_UNUSED(file);
    BRL_UNUSED(line);
    BRL_UNUSED(func);

    return realloc(ptr, size);
}

static char *_brl_default_strdup(const char *str, const char *file, int line,
                                 const char *func)
{
    BRL_UNUSED(file);
    BRL_UNUSED(line);
    BRL_UNUSED(func);

    return strdup(str);
}

static void _brl_default_free(void *ptr,
                              const char *file, int line, const char *func)
{
    BRL_UNUSED(file);
    BRL_UNUSED(line);
    BRL_UNUSED(func);

    free(ptr);
}


/* By default we use the libc memory allocator. */
brl_allocator_t __brl_mm = {
    .allocfn   = _brl_default_alloc,
    .reallocfn = _brl_default_realloc,
    .strdupfn  = _brl_default_strdup,
    .freefn    = _brl_default_free
};

/* Once an allocation is done, this will block changing the allocator. */
int __brl_mm_busy = FALSE;


int brl_set_allocator(brl_allocator_t *allocator)
{
    if (!__brl_mm_busy) {
        __brl_mm = *allocator;

        return 0;
    }
    else {
        errno = EBUSY;

        return -1;
    }
}
