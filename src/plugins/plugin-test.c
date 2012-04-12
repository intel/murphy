#include <murphy/core/plugin.h>

enum {
    STRING1,
    STRING2,
    BOOLEAN1,
    BOOLEAN2,
    UINT321,
    INT321,
    DOUBLE1,
};


static int test_init(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *arg, *args;
    int               i;

    mrp_log_info("%s() called for test instance '%s'...", __FUNCTION__,
		 plugin->instance);

    for (i = 0, arg = plugin->args; i < plugin->descriptor->narg; i++, arg++) {
	switch (arg->type) {
	case MRP_PLUGIN_ARG_TYPE_STRING:
	    printf("string argument: %s=%s\n", arg->key, arg->str);
	    break;
	case MRP_PLUGIN_ARG_TYPE_BOOL:
	    printf("boolean argument: %s=%d\n", arg->key, arg->bln);
	    break;
	case MRP_PLUGIN_ARG_TYPE_UINT32:
	    printf("string argument: %s=%u\n", arg->key, arg->u32);
	    break;
	case MRP_PLUGIN_ARG_TYPE_INT32:
	    printf("string argument: %s=%d\n", arg->key, arg->i32);
	    break;
	case MRP_PLUGIN_ARG_TYPE_DOUBLE:
	    printf("double argument: %s=%f\n", arg->key, arg->dbl);
	    break;
	default:
	    printf("argument of invalid type 0x%x\n", arg->type);
	}
    }

    args = plugin->args;
    printf("string1:  %s=%s\n", args[STRING1].key, args[STRING1].str);
    printf("string2:  %s=%s\n", args[STRING2].key, args[STRING2].str);
    printf("boolean1: %s=%d\n", args[BOOLEAN1].key, args[BOOLEAN1].bln);
    printf("boolean2: %s=%d\n", args[BOOLEAN2].key, args[BOOLEAN2].bln);
    printf("uint32: %s=%d\n", args[UINT321].key, args[UINT321].u32);
    printf("int32: %s=%d\n", args[INT321].key, args[INT321].i32);
    printf("double: %s=%f\n", args[DOUBLE1].key, args[DOUBLE1].dbl);

    return TRUE;
}


static void test_exit(mrp_plugin_t *plugin)
{
    mrp_log_info("%s() called for test instance '%s'...", __FUNCTION__,
		 plugin->instance);
}


#define TEST_HELP "Just a load/unload test."


static mrp_plugin_arg_t args[] = {
    MRP_PLUGIN_ARGIDX(STRING1 , STRING, "string1" , "default string1"),
    MRP_PLUGIN_ARGIDX(STRING2 , STRING, "string2" , "default string2"),
    MRP_PLUGIN_ARGIDX(BOOLEAN1, BOOL  , "boolean1", TRUE             ),
    MRP_PLUGIN_ARGIDX(BOOLEAN2, BOOL  , "boolean2", FALSE            ),
    MRP_PLUGIN_ARGIDX(UINT321 , UINT32, "uint32"  , 3141             ),
    MRP_PLUGIN_ARGIDX(INT321  , INT32 , "int32"   ,-3141             ),
    MRP_PLUGIN_ARGIDX(DOUBLE1 , DOUBLE, "double"  ,-3.141            ),
};

MURPHY_REGISTER_PLUGIN("test", TEST_HELP, test_init, test_exit, args);
