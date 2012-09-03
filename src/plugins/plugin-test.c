#include <murphy/common/macros.h>
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

MRP_EXPORTABLE(static char *, method1, (int arg1, char *arg2, double arg3))
{
    MRP_UNUSED(arg1);
    MRP_UNUSED(arg2);
    MRP_UNUSED(arg3);

    mrp_log_info("%s()...", __FUNCTION__);

    return "method1 was here...";
}

static int boilerplate1(mrp_plugin_t *plugin,
                        const char *name, mrp_script_env_t *env)
{
    MRP_UNUSED(plugin);
    MRP_UNUSED(name);
    MRP_UNUSED(env);

    return -1;
}

MRP_EXPORTABLE(static int,  method2, (char *arg1, double arg2, int arg3))
{
    MRP_UNUSED(arg1);
    MRP_UNUSED(arg2);
    MRP_UNUSED(arg3);

    mrp_log_info("%s()...", __FUNCTION__);

    return 313;
}

static int boilerplate2(mrp_plugin_t *plugin,
                        const char *name, mrp_script_env_t *env)
{
    MRP_UNUSED(plugin);
    MRP_UNUSED(name);
    MRP_UNUSED(env);

    return -1;
}


MRP_IMPORTABLE(char *, method1ptr, (int arg1, char *arg2, double arg3));
MRP_IMPORTABLE(int, method2ptr, (char *arg1, double arg2, int arg3));


#if 0
static int export_methods(mrp_plugin_t *plugin)
{
    mrp_method_descr_t methods[] = {
        { "method1", "char *(int arg1, char *arg2, double arg3)",
          method1, boilerplate1, plugin },
        { "method2", "int (char *arg1, double arg2, int arg3)",
          method2, boilerplate2, plugin },
        { NULL, NULL, NULL, NULL, NULL }
    };
    mrp_method_descr_t *m;

    for (m = methods; m->name != NULL; m++)
        if (mrp_export_method(m) < 0)
            return FALSE;
        else
            mrp_log_info("Successfully exported method '%s'...", m->name);

    return TRUE;
}


static int remove_methods(mrp_plugin_t *plugin)
{
    mrp_method_descr_t methods[] = {
        { "method1", "char *(int arg1, char *arg2, double arg3)",
          method1, boilerplate1, plugin },
        { "method2", "int (char *arg1, double arg2, int arg3)",
          method2, boilerplate2, plugin },
        { NULL, NULL, NULL, NULL, NULL }
    };
    mrp_method_descr_t *m;

    for (m = methods; m->name != NULL; m++)
        if (mrp_remove_method(m) < 0)
            mrp_log_info("Failed to remove method '%s'...", m->name);
        else
            mrp_log_info("Failed to remove method '%s'...", m->name);

    return TRUE;
}


static int import_methods(mrp_plugin_t *plugin)
{
    mrp_method_descr_t methods[] = {
        { "method1", "char *(int arg1, char *arg2, double arg3)",
          method1, boilerplate1, plugin },
        { "method2", "int (char *arg1, double arg2, int arg3)",
          method2, boilerplate2, plugin },
        { NULL, NULL, NULL, NULL, NULL }
    };

    void *native_check[] = { method1, method2 };
    void *script_check[] = { boilerplate1, boilerplate2 };
    int   i;

    mrp_method_descr_t *m;

    const char    *name, *sig;
    char           buf[512];
    void          *native;
    int          (*script)(mrp_plugin_t *, const char *, mrp_script_env_t *);

    for (i = 0, m = methods; m->name != NULL; i++, m++) {
        name = m->name;
        sig  = m->signature;

        if (mrp_import_method(name, sig, &native, &script) < 0)
            return FALSE;

        if (native != native_check[i] || script != script_check[i])
            return FALSE;

        mrp_log_info("%s imported as %p, %p...", name, native, script);

        snprintf(buf, sizeof(buf), "%s.%s", plugin->instance, m->name);
        name = buf;

        if (mrp_import_method(name, sig, &native, &script) < 0)
            return FALSE;

        if (native != native_check[i] || script != script_check[i])
            return FALSE;

        mrp_log_info("%s imported as %p, %p...", name, native, script);
    }

    return TRUE;
}


static int release_methods(mrp_plugin_t *plugin)
{
    mrp_method_descr_t methods[] = {
        { "method1", "char *(int arg1, char *arg2, double arg3)",
          method1, boilerplate1, plugin },
        { "method2", "int (char *arg1, double arg2, int arg3)",
          method2, boilerplate2, plugin },
        { NULL, NULL, NULL, NULL, NULL }
    };
    const char *name, *sig;
    char        buf[512];
    mrp_method_descr_t *m;
    void *native, *natives[] = { method1     , method2      };
    int (*script)(mrp_plugin_t *, const char *, mrp_script_env_t *);
    void *scripts[] = { boilerplate1, boilerplate2 };
    int   i;

    for (i = 0, m = methods; m->name != NULL; i++, m++) {
        name   = m->name;
        sig    = m->signature;
        native = natives[i];
        script = scripts[i];

        if (mrp_release_method(name, sig, &native, &script) < 0)
            mrp_log_error("Failed to release method '%s'...", name);
        else
            mrp_log_info("Successfully released method '%s'...", name);

        snprintf(buf, sizeof(buf), "%s.%s", plugin->instance, m->name);
        name   = buf;
        native = natives[i];
        script = scripts[i];

        if (mrp_release_method(name, sig, &native, &script) < 0)
            mrp_log_error("Failed to release method '%s'...", name);
        else
            mrp_log_info("Successfully released method '%s'...", name);
    }

    return TRUE;
}

#endif

int test_imports(void)
{
    if (method1ptr == NULL || method2ptr == NULL) {
        mrp_log_error("Failed to import methods...");
        return FALSE;
    }

    mrp_log_info("method1ptr returned '%s'...", method1ptr(1, "foo", 3.141));
    mrp_log_info("method2ptr returned '%d'...", method2ptr("bar", 9.81, 2));

    return TRUE;
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

#if 0
    if (!export_methods(plugin))
        return FALSE;

    if (!import_methods(plugin))
        return FALSE;
#endif

    test_imports();


    return !args[ARG_FAILINIT].bln;
}


static void test_exit(mrp_plugin_t *plugin)
{
    mrp_log_info("%s() called for test instance '%s'...", __FUNCTION__,
                 plugin->instance);

#if 0
    release_methods(plugin);
    remove_methods(plugin);
#endif

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

static mrp_method_descr_t exports[] = {
    MRP_GENERIC_METHOD("method1", method1, boilerplate1),
    MRP_GENERIC_METHOD("method2", method2, boilerplate2),
};

static mrp_method_descr_t imports[] = {
    MRP_IMPORT_METHOD("method1", method1ptr),
    MRP_IMPORT_METHOD("method2", method2ptr),
};


MURPHY_REGISTER_PLUGIN("test",
                       TEST_VERSION, TEST_DESCRIPTION, TEST_AUTHORS, TEST_HELP,
                       MRP_MULTIPLE, test_init, test_exit,
                       args, MRP_ARRAY_SIZE(args),
                       exports, MRP_ARRAY_SIZE(exports),
                       imports, MRP_ARRAY_SIZE(imports),
                       &test_group);
