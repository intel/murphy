#include <murphy/core/plugin.h>

static int test_init(mrp_plugin_t *plugin)
{
    mrp_log_info("%s() called for test instance '%s'...", __FUNCTION__,
		 plugin->instance);
    return TRUE;
}


static void test_exit(mrp_plugin_t *plugin)
{
    mrp_log_info("%s() called for test instance '%s'...", __FUNCTION__,
		 plugin->instance);
}


#define TEST_HELP "Just a load/unload test."

MURPHY_REGISTER_PLUGIN("test", TEST_HELP, test_init, test_exit);
