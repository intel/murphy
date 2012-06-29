#ifndef __MURPHY_SIGNALLING_UTIL_H__
#define __MURPHY_SIGNALLING_UTIL_H__

#include <stdint.h>

#define signalling_info(fmt, args...)  mrp_log_info("signalling: "fmt , ## args)
#define signalling_warn(fmt, args...)  mrp_log_warning("signalling: "fmt , ## args)
#define signalling_error(fmt, args...) mrp_log_error("signalling: "fmt , ## args)

void *u_to_p(uint32_t u);
uint32_t p_to_u(const void *p);
int int_comp(const void *key1, const void *key2);
uint32_t int_hash(const void *key);

#endif /* __MURPHY_SIGNALLING_UTIL_H__ */
