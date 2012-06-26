#include <murphy/core/plugin.h>
#include <murphy/core/console.h>

enum {
    ARG_STRING1,
    ARG_STRING2,
    ARG_BOOLEAN1,
    ARG_BOOLEAN2,
    ARG_UINT321,
    ARG_INT321,
    ARG_DOUBLE1,
    ARG_FAILINIT,
    ARG_FAILEXIT,
};


void one_cb(mrp_console_t *c, void *user_data, int argc, char **argv);
void two_cb(mrp_console_t *c, void *user_data, int argc, char **argv);
void three_cb(mrp_console_t *c, void *user_data, int argc, char **argv);
void four_cb(mrp_console_t *c, void *user_data, int argc, char **argv);

MRP_CONSOLE_GROUP(test_group, "test", NULL, NULL, {
        MRP_TOKENIZED_CMD("one"  , one_cb  , TRUE,
                          "one [args]", "command 1", "description 1"),
        MRP_TOKENIZED_CMD("two"  , two_cb  , FALSE,
                          "two [args]", "command 2", "description 2"),
        MRP_TOKENIZED_CMD("three", three_cb, FALSE,
                          "three [args]", "command 3", "description 3"),
        MRP_TOKENIZED_CMD("four" , four_cb , TRUE,
                          "four [args]", "command 4", "description 4")
});


void one_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    int i;

    MRP_UNUSED(user_data);

    for (i = 0; i < argc; i++) {
        printf("%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
        mrp_console_printf(c, "%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
    }
}


void two_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    int i;

    MRP_UNUSED(user_data);

    for (i = 0; i < argc; i++) {
        printf("%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
        mrp_console_printf(c, "%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
    }
}


void three_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    int i;

    MRP_UNUSED(user_data);

    for (i = 0; i < argc; i++) {
        printf("%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
        mrp_console_printf(c, "%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
    }
}


void four_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    int i;

    MRP_UNUSED(user_data);

    for (i = 0; i < argc; i++) {
        printf("%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
        mrp_console_printf(c, "%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
    }
}




static int test_init(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *args;

    mrp_log_info("%s() called for test instance '%s'...", __FUNCTION__,
                 plugin->instance);

    args = plugin->args;
    printf(" string1:  %s\n", args[ARG_STRING1].str);
    printf(" string2:  %s\n", args[ARG_STRING2].str);
    printf("boolean1:  %s\n", args[ARG_BOOLEAN1].bln ? "TRUE" : "FALSE");
    printf("boolean2:  %s\n", args[ARG_BOOLEAN2].bln ? "TRUE" : "FALSE");
    printf("  uint32:  %u\n", args[ARG_UINT321].u32);
    printf("   int32:  %d\n", args[ARG_INT321].i32);
    printf("  double:  %f\n", args[ARG_DOUBLE1].dbl);
    printf("init fail: %s\n", args[ARG_FAILINIT].bln ? "TRUE" : "FALSE");
    printf("exit fail: %s\n", args[ARG_FAILEXIT].bln ? "TRUE" : "FALSE");

    return !args[ARG_FAILINIT].bln;
}


static void test_exit(mrp_plugin_t *plugin)
{
    mrp_log_info("%s() called for test instance '%s'...", __FUNCTION__,
                 plugin->instance);

    /*return !args[ARG_FAILINIT].bln;*/
}


#define TEST_DESCRIPTION "A primitive plugin just to test the plugin infra."
#define TEST_HELP        "Just a load/unload test."
#define TEST_VERSION     MRP_VERSION_INT(0, 0, 1)
#define TEST_AUTHORS     "D. Duck <donald.duck@ducksburg.org>"

static mrp_plugin_arg_t args[] = {
    MRP_PLUGIN_ARGIDX(ARG_STRING1 , STRING, "string1" , "default string1"),
    MRP_PLUGIN_ARGIDX(ARG_STRING2 , STRING, "string2" , "default string2"),
    MRP_PLUGIN_ARGIDX(ARG_BOOLEAN1, BOOL  , "boolean1", TRUE             ),
    MRP_PLUGIN_ARGIDX(ARG_BOOLEAN2, BOOL  , "boolean2", FALSE            ),
    MRP_PLUGIN_ARGIDX(ARG_UINT321 , UINT32, "uint32"  , 3141             ),
    MRP_PLUGIN_ARGIDX(ARG_INT321  , INT32 , "int32"   , -3141            ),
    MRP_PLUGIN_ARGIDX(ARG_DOUBLE1 , DOUBLE, "double"  , -3.141           ),
    MRP_PLUGIN_ARGIDX(ARG_FAILINIT, BOOL  , "failinit", FALSE            ),
    MRP_PLUGIN_ARGIDX(ARG_FAILEXIT, BOOL  , "failexit", FALSE            ),
};

MURPHY_REGISTER_PLUGIN("test",
                       TEST_VERSION, TEST_DESCRIPTION, TEST_AUTHORS, TEST_HELP,
                       MRP_MULTIPLE, test_init, test_exit, args, &test_group);
