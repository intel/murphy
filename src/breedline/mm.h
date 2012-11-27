#ifndef __BREEDLINE_MM_H__
#define __BREEDLINE_MM_H__

#include <stdlib.h>
#include <string.h>

/* Macro that can be used to pass the location of its usage further. */
#define BRL_LOC __FILE__, __LINE__, __func__

/* Memory allocation macros used by breedline. */
#define brl_allocz(size) ({                                        \
            void   *_ptr;                                          \
            size_t  _size = size;                                  \
                                                                   \
            if ((_ptr = __brl_mm.allocfn(_size, BRL_LOC)) != NULL) \
                memset(_ptr, 0, _size);                            \
                                                                   \
            __brl_mm_busy = TRUE;                                  \
                                                                   \
            _ptr; })

#define brl_reallocz(ptr, o, n) ({                               \
            typeof(ptr) _ptr;                                    \
            size_t      _size = sizeof(*_ptr) * (n);             \
            typeof(n)   _n    = (n);                             \
            typeof(o)   _o;                                      \
                                                                 \
            if ((ptr) != NULL)                                   \
                _o = o;                                          \
            else                                                 \
                _o = 0;                                          \
                                                                 \
            _ptr = __brl_mm.reallocfn(ptr, _size, BRL_LOC);      \
            if (_ptr != NULL || _n == 0) {                       \
                if ((unsigned)(_n) > (unsigned)(_o))             \
                    memset(_ptr + (_o), 0,                       \
                           ((_n) - (_o)) * sizeof(*_ptr));       \
                ptr = _ptr;                                      \
            }                                                    \
                                                                 \
            __brl_mm_busy = TRUE;                                \
                                                                 \
            _ptr; })

#define brl_strdup(s) ({                                         \
            __brl_mm_busy = TRUE;                                \
                                                                 \
            __brl_mm.strdupfn((s), BRL_LOC);                     \
        })

#define brl_free(ptr) __brl_mm.freefn((ptr), BRL_LOC)

extern brl_allocator_t __brl_mm;
extern int __brl_mm_busy;

#endif /* __BREEDLINE_MM_H__ */
