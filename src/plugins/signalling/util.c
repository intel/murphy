#include <sys/types.h>

#include "util.h"

void *u_to_p(uint32_t u)
{
#ifdef __SIZEOF_POINTER__
#if __SIZEOF_POINTER__ == 8
    uint64_t o = u;
#else
    uint32_t o = u;
#endif
#else
    uint32_t o = o;
#endif
    return (void *) o;
}

uint32_t p_to_u(const void *p)
{
#ifdef __SIZEOF_POINTER__
#if __SIZEOF_POINTER__ == 8
    uint32_t o = 0;
    uint64_t big = (uint64_t) p;
    o = big & 0xffffffff;
#else
    uint32_t o = (uint32_t) p;
#endif
#else
    uint32_t o = p;
#endif
    return o;
}


int int_comp(const void *key1, const void *key2)
{
    return key1 != key2;
}


uint32_t int_hash(const void *key)
{
    return p_to_u(key);
}
