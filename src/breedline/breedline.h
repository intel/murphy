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
    void *(*alloc)(size_t size, const char *file, int line, const char *func);
    void *(*realloc)(void *ptr, size_t size, const char *file, int line,
                     const char *func);
    char *(*strdup)(const char *str, const char *file, int line,
                    const char *func);
    void  (*free)(void *ptr, const char *file, int line, const char *func);
} brl_allocator_t;

/** Override the default memory allocator. */
int brl_set_allocator(brl_allocator_t *allocator);

BRL_CDECL_END

#endif /* __BREEDLINE_H__ */
