/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/core/plugin.h>
#include <murphy/core/console.h>
#include <murphy/core/event.h>

#include <murphy-db/mql.h>
#include <murphy-db/mqi.h>

#include <murphy/plugins/signalling/signalling.h>
#include <murphy/plugins/signalling/signalling-protocol.h>



typedef struct {
    mrp_event_watch_t *w;
} test_data_t;


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
void resolve_cb(mrp_console_t *c, void *user_data, int argc, char **argv);
void signalling_cb_1(mrp_console_t *c, void *user_data, int argc, char **argv);
void signalling_cb_2(mrp_console_t *c, void *user_data, int argc, char **argv);
void signalling_cb_3(mrp_console_t *c, void *user_data, int argc, char **argv);
void signalling_info_register_cb(mrp_console_t *c, void *user_data, int argc, char **argv);
void signalling_info_unregister_cb(mrp_console_t *c, void *user_data, int argc, char **argv);
void signalling_create_ep_cb(mrp_console_t *c, void *user_data, int argc, char **argv);


MRP_CONSOLE_GROUP(test_group, "test", NULL, NULL, {
        MRP_TOKENIZED_CMD("one"  , one_cb  , TRUE,
                          "one [args]", "command 1", "description 1"),
        MRP_TOKENIZED_CMD("two"  , two_cb  , FALSE,
                          "two [args]", "command 2", "description 2"),
        MRP_TOKENIZED_CMD("three", three_cb, FALSE,
                          "three [args]", "command 3", "description 3"),
        MRP_TOKENIZED_CMD("four" , four_cb , TRUE,
                          "four [args]", "command 4", "description 4"),
        MRP_TOKENIZED_CMD("update" , resolve_cb , TRUE,
                          "update <target>", "update target", "update target"),
        MRP_TOKENIZED_CMD("signalling_1" , signalling_cb_1 , TRUE,
                          "signalling_1 [args]", "signalling command",
                          "Signalling test case 1"),
        MRP_TOKENIZED_CMD("signalling_2" , signalling_cb_2 , TRUE,
                          "signalling_2 [args]", "signalling command",
                          "Signalling test case 2"),
        MRP_TOKENIZED_CMD("signalling_3" , signalling_cb_3 , TRUE,
                          "signalling_3 [args]", "signalling command",
                          "Signalling test case 3"),
        MRP_TOKENIZED_CMD("signalling_info_register",
                          signalling_info_register_cb , TRUE,
                          "signalling_info_register [args]",
                          "signalling back channel registration command",
                          "Signalling back channel registration"),
        MRP_TOKENIZED_CMD("signalling_info_unregister",
                          signalling_info_unregister_cb , TRUE,
                          "signalling_info_unregister [args]",
                          "signalling back channel unregistration command",
                          "Signalling back channel unregistration"),
        MRP_TOKENIZED_CMD("signalling_create_ep_cb",
                          signalling_create_ep_cb, TRUE,
                          "signalling_create_ep_cb [args]",
                          "signalling internal EP creation command",
                          "Create internal enforcement point for signalling")
});


void one_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    int i;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    for (i = 0; i < argc; i++) {
        printf("%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
    }
}


void two_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    int i;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    for (i = 0; i < argc; i++) {
        printf("%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
    }
}


void three_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    int i;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    for (i = 0; i < argc; i++) {
        printf("%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
    }
}


void four_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    int i;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    for (i = 0; i < argc; i++) {
        printf("%s(): #%d: '%s'\n", __FUNCTION__, i, argv[i]);
    }
}


void resolve_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    mrp_context_t  *ctx = c->ctx;
    const char     *target;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    if (argc == 3) {
        target = argv[2];

        if (ctx->r != NULL) {
            if (mrp_resolver_update_target(ctx->r, target, NULL) > 0)
                printf("'%s' updated OK.\n", target);
            else
                printf("Failed to update '%s'.\n", target);
        }
        else
            printf("Resolver/ruleset is not available.\n");
    }
    else {
        printf("Usage: %s %s <target-name>\n", argv[0], argv[1]);
    }
}


MRP_EXPORTABLE(char *, method1, (int arg1, char *arg2, double arg3))
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

    method1(1, "foo", 9.81);

    return TRUE;
}

MRP_EXPORTABLE(int, method2, (char *arg1, double arg2, int arg3))
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

MRP_IMPORTABLE(uint32_t, mrp_tx_open_signal, ());
MRP_IMPORTABLE(int, mrp_tx_add_domain, (uint32_t id, const char *domain));
MRP_IMPORTABLE(int, mrp_tx_add_data, (uint32_t id, const char *row));
MRP_IMPORTABLE(void, mrp_tx_add_success_cb, (uint32_t id, mrp_tx_success_cb cb, void *data));
MRP_IMPORTABLE(void, mrp_tx_add_error_cb, (uint32_t id, mrp_tx_error_cb cb, void *data));
MRP_IMPORTABLE(int, mrp_tx_close_signal, (uint32_t id));
MRP_IMPORTABLE(void, mrp_tx_cancel_signal, (uint32_t id));

MRP_IMPORTABLE(int, mrp_info_register, (const char *client_id, mrp_info_cb cb, void *data));
MRP_IMPORTABLE(int, mrp_info_unregister, (const char *client_id));

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


static void event_cb(mrp_event_watch_t *w, int id, mrp_msg_t *event_data,
                     void *user_data)
{
    mrp_plugin_t *plugin = (mrp_plugin_t *)user_data;

    MRP_UNUSED(w);

    mrp_log_info("%s: got event 0x%x (%s):", plugin->instance, id,
                 mrp_get_event_name(id));
    mrp_msg_dump(event_data, stdout);
}


static int subscribe_events(mrp_plugin_t *plugin)
{
    test_data_t      *data = (test_data_t *)plugin->data;
    mrp_event_mask_t  events;

    mrp_set_named_events(&events,
                         MRP_PLUGIN_EVENT_LOADED,
                         MRP_PLUGIN_EVENT_STARTED,
                         MRP_PLUGIN_EVENT_FAILED,
                         MRP_PLUGIN_EVENT_STOPPING,
                         MRP_PLUGIN_EVENT_STOPPED,
                         MRP_PLUGIN_EVENT_UNLOADED,
                         NULL);

    data->w = mrp_add_event_watch(&events, event_cb, plugin);

    return (data->w != NULL);
}


static void unsubscribe_events(mrp_plugin_t *plugin)
{
    test_data_t *data = (test_data_t *)plugin->data;

    mrp_del_event_watch(data->w);
    data->w = NULL;
}


static void success_cb(uint32_t tx, void *data)
{
    mrp_console_t *c = data;

    MRP_UNUSED(c);

    printf("%s(): transaction %u\n", __FUNCTION__, tx);
}


static void error_cb(uint32_t tx, mrp_tx_error_t err, void *data)
{
    mrp_console_t *c = data;

    MRP_UNUSED(c);

    printf("%s(): transaction %u error: %s\n", __FUNCTION__,
           tx, (err == MRP_TX_ERROR_NACKED) ? "NACK" : "no reply");
}


void signalling_cb_1(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    uint32_t tx;

    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    tx = mrp_tx_open_signal();

    mrp_tx_add_domain(tx, "domain1");

    mrp_tx_add_data(tx, "this is a data row");
    mrp_tx_add_data(tx, "this is another data row");

    mrp_tx_add_success_cb(tx, success_cb, c);
    mrp_tx_add_error_cb(tx, error_cb, c);

    mrp_tx_close_signal(tx);
}


void signalling_cb_2(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    uint32_t tx;

    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    tx = mrp_tx_open_signal();

    mrp_tx_add_domain(tx, "domain_nonexistent");

    mrp_tx_add_data(tx, "this is a data row");
    mrp_tx_add_data(tx, "this is another data row");

    mrp_tx_add_success_cb(tx, success_cb, c);
    mrp_tx_add_error_cb(tx, error_cb, c);

    mrp_tx_close_signal(tx);
}

void signalling_cb_3(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    uint32_t tx;
    int ret;

    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    tx = mrp_tx_open_signal();

    mrp_tx_add_domain(tx, "domain1");

    mrp_tx_add_data(tx, "this is a data row");
    mrp_tx_add_data(tx, "this is another data row");

    mrp_tx_add_success_cb(tx, success_cb, c);
    mrp_tx_add_error_cb(tx, error_cb, c);

    /* try cancelling the signal first */
    mrp_tx_cancel_signal(tx);

    ret = mrp_tx_close_signal(tx);

    printf("%s(): tried to send a cancelled transction %u -- success %i\n",
           __FUNCTION__, tx, ret);
}

static void info_cb(char *msg, void *data)
{
    mrp_console_t *c = data;

    MRP_UNUSED(c);

    printf("received msg '%s'\n", msg);
}

void signalling_info_register_cb(mrp_console_t *c, void *user_data,
            int argc, char **argv)
{
    /* create the back channel to the test ep */

    char *ep = "foobar";
    int ret;

    MRP_UNUSED(user_data);

    if (argc == 1)
        ep = argv[0];


    ret = mrp_info_register(ep, info_cb, c);

    if (ret < 0)
        printf("Failed to register back channel to EP '%s'\n", ep);
    else
        printf("Registered back channel to EP '%s'\n", ep);
}


void signalling_info_unregister_cb(mrp_console_t *c, void *user_data,
            int argc, char **argv)
{
    /* create the back channel to the test ep */

    char *ep = "foobar";

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    if (argc == 1)
        ep = argv[0];

    mrp_info_unregister(ep);

    printf("Unregistered back channel to EP '%s'\n", ep);
}


static void dump_decision(mrp_console_t *c, ep_decision_t *msg)
{
    uint i;

    MRP_UNUSED(c);

    printf("Message contents:\n");
    for (i = 0; i < msg->n_rows; i++) {
        printf("row %d: '%s'\n", i+1, msg->rows[i]);
    }
    printf("%s required.\n\n",
                msg->reply_required ? "Reply" : "No reply");
}


static void recvfrom_evt(mrp_transport_t *t, void *data, uint16_t tag,
             mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    mrp_console_t *c = user_data;

    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);

    printf("Received message (0x%02x)\n", tag);

    switch (tag) {
        case TAG_POLICY_DECISION:
        {
            ep_decision_t *msg = data;
            dump_decision(c, msg);


            if (msg->reply_required) {
                ep_ack_t reply;

                reply.id = msg->id;
                reply.success = EP_ACK;
                mrp_transport_senddata(t, &reply, TAG_ACK);
            }
            break;
        }
        case TAG_ERROR:
            printf("Server sends an error message!\n");
            break;
        default:
            /* no other messages supported ATM */
            break;
    }

    mrp_data_free(data, tag);
}


static void recv_evt(mrp_transport_t *t, void *data, uint16_t tag, void *user_data)
{
    recvfrom_evt(t, data, tag, NULL, 0, user_data);
}


static void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    MRP_UNUSED(t);
    MRP_UNUSED(error);
    MRP_UNUSED(user_data);

    printf("Received closed event\n");
}


void signalling_create_ep_cb(mrp_console_t *c, void *user_data,
            int argc, char **argv)
{
    mrp_transport_t *t;
    static mrp_transport_evt_t evt;
    int ret, flags;
    char *domains[] = { "domain1" };

    MRP_UNUSED(user_data);

    ep_register_t msg;

    socklen_t alen;
    mrp_sockaddr_t addr;

    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    evt.closed = closed_evt;
    evt.recvdata = recv_evt;
    evt.recvdatafrom = recvfrom_evt;

    flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_MODE_CUSTOM;

    t = mrp_transport_create(c->ctx->ml, "internal", &evt, c, flags);

    alen = mrp_transport_resolve(NULL, "internal:signalling", &addr, sizeof(addr), NULL);

    if (alen <= 0) {
        printf("Error: resolving address failed!\n");
        return;
    }

    ret = mrp_transport_connect(t, &addr, alen);
    if (ret == 0) {
        printf("Error: connect failed!\n");
        return;
    }

    msg.ep_name = "ep_name";
    msg.domains = domains;
    msg.n_domains = 1;

    ret = mrp_transport_senddata(t, &msg, TAG_REGISTER);

    if (!ret) {
        printf("Failed to send register message\n");
        return;
    }
}


static int test_init(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *args;
    test_data_t      *data;

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

    data = mrp_allocz(sizeof(*data));

    if (data == NULL) {
        mrp_log_error("Failed to allocate private data for test plugin "
                      "instance %s.", plugin->instance);
        return FALSE;
    }
    else
        plugin->data = data;

    test_imports();

    subscribe_events(plugin);

    mqi_open();

    return !args[ARG_FAILINIT].bln;
}


static void test_exit(mrp_plugin_t *plugin)
{
    mrp_log_info("%s() called for test instance '%s'...", __FUNCTION__,
                 plugin->instance);

    unsubscribe_events(plugin);

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
    MRP_IMPORT_METHOD("mrp_tx_open_signal", mrp_tx_open_signal),
    MRP_IMPORT_METHOD("mrp_tx_add_domain", mrp_tx_add_domain),
    MRP_IMPORT_METHOD("mrp_tx_add_data", mrp_tx_add_data),
    MRP_IMPORT_METHOD("mrp_tx_add_success_cb", mrp_tx_add_success_cb),
    MRP_IMPORT_METHOD("mrp_tx_add_error_cb", mrp_tx_add_error_cb),
    MRP_IMPORT_METHOD("mrp_tx_close_signal", mrp_tx_close_signal),
    MRP_IMPORT_METHOD("mrp_tx_cancel_signal", mrp_tx_cancel_signal),
};


MURPHY_REGISTER_PLUGIN("test",
                       TEST_VERSION, TEST_DESCRIPTION, TEST_AUTHORS, TEST_HELP,
                       MRP_MULTIPLE, test_init, test_exit,
                       args, MRP_ARRAY_SIZE(args),
                       exports, MRP_ARRAY_SIZE(exports),
                       imports, MRP_ARRAY_SIZE(imports),
                       &test_group);
