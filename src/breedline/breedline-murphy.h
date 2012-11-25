#ifndef __BREEDLINE_MURPHY_H__
#define __BREEDLINE_MURPHY_H__

#include <murphy/common/mainloop.h>
#include <breedline/macros.h>
#include <breedline/breedline.h>

BRL_CDECL_BEGIN

brl_t *brl_create_with_murphy(int fd, const char *prompt, mrp_mainloop_t *ml,
                              brl_line_cb_t cb, void *user_data);

BRL_CDECL_END

#endif /* __BREEDLINE_MURPHY_H__ */
