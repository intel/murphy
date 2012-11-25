#ifndef __BREEDLINE_MURPHY_H__
#define __BREEDLINE_MURPHY_H__

#include <glib.h>
#include <breedline/macros.h>
#include <breedline/breedline.h>

BRL_CDECL_BEGIN

brl_t *brl_create_with_glib(int fd, const char *prompt, GMainLoop *ml,
                            brl_line_cb_t cb, void *user_data);

BRL_CDECL_END



#endif /* __BREEDLINE_MURPHY_H__ */
