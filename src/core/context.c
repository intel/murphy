#include <murphy/common/list.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/core/context.h>
#include <murphy/core/console-priv.h>

mrp_context_t *mrp_context_create(void)
{
    mrp_context_t *c;

    if ((c = mrp_allocz(sizeof(*c))) != NULL) {
        mrp_list_init(&c->plugins);
        console_setup(c);

        if ((c->ml = mrp_mainloop_create()) == NULL) {
            mrp_log_error("Failed to create mainloop.");
            mrp_free(c);
            c = NULL;
        }
    }

    return c;
}


void mrp_context_destroy(mrp_context_t *c)
{
    if (c != NULL) {
        console_cleanup(c);
        mrp_mainloop_destroy(c->ml);
        mrp_free(c);
    }


}

