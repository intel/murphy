#ifndef __MURPHY_MACROS_H__
#define __MURPHY_MACROS_H__

#include <stddef.h>

#ifndef FALSE
#    define FALSE 0
#    define TRUE (!FALSE)
#endif

/** Align ptr to multiple of align. */
#define MRP_ALIGN(ptr, align) (((ptr) + ((align)-1)) & ~((align)-1))

/** Get the offset of the given member in a struct/union of the given type. */
#define MRP_OFFSET(type, member) ((ptrdiff_t)(&(((type *)0)->member)))

/** Determine the dimension of the given array. */
#define MRP_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#ifndef __GNUC__
#  define __FUNCTION__ __func__
#endif

#ifdef __GNUC__
     /** MAX that evalutes its arguments only once. */
#    define MRP_MAX(a, b) ({                                              \
            typeof(a) _a = (a);                                           \
            typeof(b) _b = (b);                                           \
            _a > _b ? _a : _b;                                            \
        })

     /** MIN that evalutes its arguments only once. */
#    define MRP_MIN(a, b) ({                                              \
            typeof(a) _a = (a), _b = (b);                                 \
            _a < _b ? _a : _b;                                            \
        })

     /** Likeliness branch-prediction hint for the compiler. */
#    define MRP_LIKELY(cond)   __builtin_expect((cond), 1)

     /** Unlikeliness branch-prediction hint for the compiler. */
#    define MRP_UNLIKELY(cond) __builtin_expect((cond), 0)

     /** Prevent symbol from being exported (overriden by linker scripts). */
#    define MRP_HIDDEN __attribute__ ((visibility ("hidden")))

     /** Request a symbol to be exported (overriden by linker scripts). */
#    define MRP_EXPORT __attribute__ ((visibility ("default")))

     /** Ask the compiler to check for the presence of a NULL-sentinel. */
#    define MRP_NULLTERM __attribute__((sentinel))

     /** Ask for printf-like format string checks of calls to this function. */
#    define MRP_PRINTF_LIKE(format_idx, first_arg_idx)                    \
         __attribute__ ((format (printf, format_idx, first_arg_idx)))

     /** Mark a function to be called before main is entered. */
#    define MRP_INIT __attribute__((constructor))
#    define MRP_INIT_AT(prio) __attribute__ ((constructor(prio)))

     /** Mark a function to be called after main returns, or exit is called. */
#    define MRP_EXIT __attribute__ ((destructor))
#    define MRP_EXIT_AT(prio) __attribute__ ((destructor(prio)))

/** Mark a variable unused. */
#    define MRP_UNUSED(var) (void)var
#else /* ! __GNUC__ */
#    define MRP_LIKELY(cond)   (cond)
#    define MRP_UNLIKELY(cond) (cond)
#    define MRP_HIDDEN
#    define MRP_EXPORT
#    define MRP_NULLTERM
#    define MRP_PRINTF_LIKE(format_idx, first_arg_idx)
#    define __FUNCTION__ __func__
#    define MRP_INIT
#    define MRP_INIT_AT
#    define MRP_EXIT
#    define MRP_EXIT_AT
#    define MRP_UNUSED(var)
#endif

/** Macro that can be used to pass the location of it's usage. */
#    define __LOC__ __FILE__, __LINE__, __FUNCTION__

/** Assertions. */
#ifndef NDEBUG
#    define MRP_ASSERT(expr, msg) do {                                    \
        if (!(expr)) {                                                    \
            printf("assertion '%s' failed at %s@%s:%d: %s\n", #expr,      \
                   __FUNCTION__, __FILE__, __LINE__, msg);                \
            abort();                                                      \
        }                                                                 \
    } while (0)
#else
#    define MRP_ASSERT(expr, msg) do { } while (0)
#endif

/** Create a version integer from a (major, minor, micro) tuple. */
#define MRP_VERSION_INT(maj, min, mic)                                    \
    ((((maj) & 0xff) << 16) | (((min) & 0xff) << 8) | ((mic) & 0xff))

/** Create a version string from a (const) (major, minor, micro) tuple.  */
#define MRP_VERSION_STRING(maj, min, mic) #maj"."#min"."#mic

/** Extract major version from a version integer. */
#define MRP_VERSION_MAJOR(ver) (((ver) >> 16) & 0xff)

/** Extract minor version from a version integer. */
#define MRP_VERSION_MINOR(ver) (((ver) >>  8) & 0xff)

/** Extract micro version from a version integer. */
#define MRP_VERSION_MICRO(ver) ((ver) & 0xff)

/** Macro to stringify a macro argument. */
#define MRP_STRINGIFY(arg) #arg

/** C++-compatibility macros. */
#ifdef __cplusplus
#    define MRP_CDECL_BEGIN extern "C" {
#    define MRP_CDECL_END   }
#else
#    define MRP_CDECL_BEGIN
#    define MRP_CDECL_END
#endif

#endif /* __MURPHY_MACROS_H__ */

