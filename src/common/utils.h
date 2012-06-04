#ifndef __MURPHY_UTILS_H__
#define __MURPHY_UTILS_H__

#include <stdint.h>

int mrp_daemonize(const char *dir, const char *new_out, const char *new_err);

int mrp_string_comp(const void *key1, const void *key2);
uint32_t mrp_string_hash(const void *key);

#endif /* __MURPHY_UTILS_H__ */
