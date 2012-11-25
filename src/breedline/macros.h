#ifndef __BREEDLINE_MACROS_H__
#define __BREEDLINE_MACROS_H__

#define BRL_UNUSED(var) (void)var

#ifdef __GNUC__
#    define BRL_UNLIKELY(cond) __builtin_expect((cond), 0)
#    define BRL_LIKELY(cond)   __builtin_expect((cond), 1)
#else
#    define BRL_UNLIKELY(cond) (cond)
#    define BRL_LIKELY(cond)   (cond)
#endif

#ifdef __cplusplus
#    define BRL_CDECL_BEGIN extern "C" {
#    define BRL_CDECL_END   }
#else
#    define BRL_CDECL_BEGIN
#    define BRL_CDECL_END
#endif

#endif /* __BREEDLINE_MACROS_H__ */
