#ifndef __MURPHY_DEBUG_INFO_H__
#define __MURPHY_DEBUG_INFO_H__

#include <murphy/common/list.h>

/*
 * line number information for a single function
 */

typedef struct {
    const char *func;                    /* name of the function */
    int         line;                    /* start at this line */
} mrp_debug_info_t;


/*
 * funcion - line number mapping for a single file
 */

typedef struct {
    mrp_list_hook_t   hook;              /* hook for startup registration */
    const char       *file;              /* file name */
    mrp_debug_info_t *info;              /* function information */
} mrp_debug_file_t;


#endif /* __MURPHY_DEBUG_INFO_H__ */
