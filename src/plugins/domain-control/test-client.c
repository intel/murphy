/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/socket.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <murphy/common.h>
#include <breedline/breedline-murphy.h>

#include "client.h"

#define DEFAULT_PROMPT "test-controller"


/*
 * client context
 */

typedef struct {
    const char      *addrstr;            /* server address */
    int              zone;               /* run in zone control mode */
    int              verbose;            /* verbose mode */
    int              audio;              /* subscribe for audio_playback_* */
    mrp_mainloop_t  *ml;                 /* murphy mainloop */
    void            *dc;                 /* domain controller */
    brl_t           *brl;                /* breedline for terminal input */
} client_t;


#define NVALUE 512


/*
 * device and stream definitions
 */

#define NDEVICE        (MRP_ARRAY_SIZE(devices) - 1)
#define DEVICE_NCOLUMN 4

typedef struct {
    const char *name;
    const char *type;
    int         public;
    int         available;
} device_t;

static device_t devices[] = {
    { "builtin-speaker" , "speaker"  , TRUE , TRUE  },
    { "builtin-earpiece", "speaker"  , FALSE, TRUE  },
    { "usb-speaker"     , "speaker"  , TRUE , FALSE },
    { "a2dp-speaker"    , "speaker"  , TRUE , FALSE },
    { "wired-headset"   , "headset"  , FALSE, FALSE },
    { "usb-headphone"   , "headphone", FALSE, FALSE },
    { "a2dp-headphone"  , "headphone", FALSE, FALSE },
    { "sco-headset"     , "headset"  , FALSE, FALSE },
    { NULL              , NULL       , FALSE, FALSE }
};

#define NSTREAM        (MRP_ARRAY_SIZE(streams) - 1)
#define STREAM_NCOLUMN 4

typedef struct {
    const char *name;
    const char *role;
    pid_t       owner;
    int         playing;
} stream_t;

static stream_t streams[] = {
    { "player1", "player"   , 1234, FALSE },
    { "player2", "player"   , 4321, FALSE },
    { "navit"  , "navigator", 5432, FALSE },
    { "phone"  , "call"     , 6666, FALSE },
    { NULL     , NULL       , 0   , FALSE }
};


/*
 * device and stream descriptors
 */

#define DEVICE_COLUMNS          \
    "name      varchar(32), "   \
    "type      varchar(32), "   \
    "public    integer    , "   \
    "available integer"

#define DEVICE_INDEX "name"

#define DEVICE_SELECT "*"

#define DEVICE_WHERE  NULL

#define STREAM_COLUMNS          \
    "name      varchar(32),"    \
    "role      varchar(32),"    \
    "owner     unsigned   ,"    \
    "playing   integer"

#define STREAM_INDEX "name"

#define STREAM_SELECT "*"
#define STREAM_WHERE  NULL

#define SELECT_ALL "*"
#define ANY_WHERE  NULL

mrp_domctl_table_t media_tables[] = {
    MRP_DOMCTL_TABLE("test-devices", DEVICE_COLUMNS, DEVICE_INDEX),
    MRP_DOMCTL_TABLE("test-streams", STREAM_COLUMNS, STREAM_INDEX),
};

mrp_domctl_watch_t media_watches[] = {
    MRP_DOMCTL_WATCH("test-devices", DEVICE_SELECT, DEVICE_WHERE, 0),
    MRP_DOMCTL_WATCH("test-streams", STREAM_SELECT, STREAM_WHERE, 0),
    MRP_DOMCTL_WATCH("audio_playback_owner", SELECT_ALL, ANY_WHERE, 0),
    MRP_DOMCTL_WATCH("audio_playback_users", SELECT_ALL, ANY_WHERE, 0),
};


/*
 * zone and call definitions
 */

#define NZONE        (MRP_ARRAY_SIZE(zones) - 1)
#define ZONE_NCOLUMN 3

typedef struct {
    const char *name;
    int         occupied;
    int         active;
} zone_t;

static zone_t zones[] = {
    { "driver"     , TRUE , FALSE },
    { "fearer"     , FALSE, TRUE  },
    { "back-left"  , TRUE , FALSE },
    { "back-center", FALSE, FALSE },
    { "back-right" , TRUE , TRUE  },
    { NULL         , FALSE, FALSE }
};


#define NCALL        (MRP_ARRAY_SIZE(calls) - 1)
#define CALL_NCOLUMN 3

typedef struct {
    int         id;
    const char *state;
    const char *modem;
} call_t;

static call_t calls[] = {
    { 1, "active"  , "modem1" },
    { 2, "ringing" , "modem1" },
    { 3, "held"    , "modem2" },
    { 4, "alerting", "modem2" },
    { 0, NULL      , NULL     }
};


/*
 * zone and call descriptors
 */

#define ZONE_COLUMNS          \
    "name      varchar(32), " \
    "occupied  integer    , " \
    "active    integer"

#define ZONE_INDEX "name"

#define ZONE_SELECT "*"

#define ZONE_WHERE  NULL

#define CALL_COLUMNS          \
    "id        integer    , " \
    "state     varchar(32), " \
    "modem     varchar(32)"

#define CALL_INDEX "id"

#define CALL_SELECT "*"

#define CALL_WHERE  NULL

mrp_domctl_table_t zone_tables[] = {
    MRP_DOMCTL_TABLE("test-zones", ZONE_COLUMNS, ZONE_INDEX),
    MRP_DOMCTL_TABLE("test-calls", CALL_COLUMNS, CALL_INDEX),
};

mrp_domctl_watch_t zone_watches[] = {
    MRP_DOMCTL_WATCH("test-zones", ZONE_SELECT, ZONE_WHERE, 0),
    MRP_DOMCTL_WATCH("test-calls", CALL_SELECT, CALL_WHERE, 0),
    MRP_DOMCTL_WATCH("audio_playback_owner", SELECT_ALL, ANY_WHERE, 0),
    MRP_DOMCTL_WATCH("audio_playback_users", SELECT_ALL, ANY_WHERE, 0),
};

mrp_domctl_table_t *exports;
int                 nexport;
mrp_domctl_watch_t *imports;
int                 nimport;


static client_t *client;


static void fatal_msg(int error, const char *format, ...);
static void error_msg(const char *format, ...);
static void info_msg(const char *format, ...);

static void export_data(client_t *c);


static void plug_device(client_t *c, const char *name, int plug)
{
    device_t *d;
    int       changed;

    if (c->zone) {
        error_msg("cannot plug/unplug, client is in zone mode");
        return;
    }

    changed = FALSE;

    for (d = devices; d->name != NULL; d++) {
        if (!strcmp(d->name, name)) {
            changed = plug ^ d->available;
            d->available = plug;
            break;
        }
    }

    if (changed) {
        info_msg("device '%s' is now %splugged", d->name, plug ? "" : "un");
        export_data(c);
    }
}


static void list_devices(void)
{
    device_t *d;
    int       n;

    for (d = devices, n = 0; d->name != NULL; d++, n++) {
        info_msg("device '%s': (%s, %s), %s",
                 d->name, d->type, d->public ? "public" : "private",
                 d->available ? "available" : "currently unplugged");
    }

    if (n == 0)
        info_msg("devices: none");
}


static void play_stream(client_t *c, const char *name, int play)
{
    stream_t *s;
    int       changed;

    if (c->zone) {
        error_msg("cannot control streams, client is in zone mode");
        return;
    }

    changed = FALSE;

    for (s = streams; s->name != NULL; s++) {
        if (!strcmp(s->name, name)) {
            changed = play ^ s->playing;
            s->playing = play;
            break;
        }
    }

    if (changed) {
        info_msg("stream '%s' is now %s", s->name, play ? "playing":"stopped");
        export_data(c);
    }
}


static void list_streams(void)
{
    stream_t *s;
    int       n;

    for (s = streams, n = 0; s->name != NULL; s++, n++) {
        info_msg("stream '%s': role %s, owner %u, currently %splaying",
                 s->name, s->role, s->owner, s->playing ? "" : "not ");
    }

    if (n == 0)
        info_msg("streams: none");
}


static void set_zone_state(client_t *c, const char *config)
{
    zone_t *z;
    int     occupied, active, changed, len;
    char    name[256], *end;

    if (!c->zone) {
        error_msg("cannot control zones, client is not in zone mode");
        return;
    }

    while (*config == ' ' || *config == '\t')
        config++;

    end = strchr(config, ' ');
    if (end == NULL)
        return;

    len = end - config;
    strncpy(name, config, len);
    name[len] = '\0';

    config = end + 1;
    while (*config == ' ' || *config == '\t')
        config++;

    occupied = FALSE;
    active   = FALSE;
    changed  = FALSE;

    if (strstr(config, "occupied"))
        occupied = TRUE;
    if (strstr(config, "active"))
        active = TRUE;

    for (z = zones; z->name != NULL; z++) {
        if (!strcmp(z->name, name)) {
            changed     = (active ^ z->active) | (occupied ^ z->occupied);
            z->active   = active;
            z->occupied = occupied;
            break;
        }
    }

    if (changed) {
        info_msg("zone '%s' is now %s and %s", z->name,
                 z->occupied ? "occupied" : "free",
                 z->active   ? "active"   : "idle");
        export_data(c);
    }
}


static void list_zones(void)
{
    zone_t *z;
    int     n;

    for (z = zones, n = 0; z->name != NULL; z++, n++) {
        info_msg("zone '%s' is now %s and %s", z->name,
                 z->occupied ? "occupied" : "free",
                 z->active   ? "active"   : "idle");
    }

    if (n == 0)
        info_msg("zones: none");
}


static void set_call_state(client_t *c, const char *config)
{
    call_t *call;
    char   idstr[64], *state, *end;
    int     id, changed, len;

    if (!c->zone) {
        error_msg("cannot control calls, client is not in zone mode");
        return;
    }

    while (*config == ' ' || *config == '\t')
        config++;

    end = strchr(config, ' ');
    if (end == NULL)
        return;

    len = end - config;
    strncpy(idstr, config, len);
    idstr[len] = '\0';

    config = end + 1;
    while (*config == ' ' || *config == '\t')
        config++;
    state = (char *)config;

    id = strtoul(idstr, &end, 10);

    if (end && *end) {
        error_msg("invalid call id '%s'", idstr);
        return;
    }

    changed = FALSE;
    for (call = calls; call->id > 0; call++) {
        if (call->id == id) {
            if (strcmp(call->state, state)) {
                mrp_free((char *)call->state);
                call->state = mrp_strdup(state);
                changed     = TRUE;
                break;
            }
        }
    }

    if (changed) {
        info_msg("call #%d is now %s", call->id, call->state);
        export_data(c);
    }
}


static void list_calls(void)
{
    call_t *c;
    int     n;

    for (c = calls, n = 0; c->id > 0; c++, n++) {
        info_msg("call #%d: %s (on modem %s)", c->id, c->state, c->modem);
    }

    if (n == 0)
        info_msg("calls: none");
}


static void init_devices(void)
{
    mrp_clear(&devices);
}


static void reset_devices(void)
{
    int i;

    for (i = 0; i < (int)MRP_ARRAY_SIZE(devices); i++) {
        mrp_free((char *)devices[i].name);
        mrp_free((char *)devices[i].type);
    }

    mrp_clear(&devices);
}


void update_devices(mrp_domctl_data_t *data)
{
    device_t           *d;
    mrp_domctl_value_t *v;
    int                 i;

    if (data->nrow != 0 && data->ncolumn != DEVICE_NCOLUMN) {
        error_msg("incorrect number of columns in device update (%d != %d)",
                  data->ncolumn, DEVICE_NCOLUMN);
        return;
    }

    if (data->nrow > (int)NDEVICE) {
        error_msg("too many rows (%d) in device update", data->nrow);
        return;
    }

    if (data->nrow == 0)
        reset_devices();
    else {
        d = devices;

        for (i = 0; i < data->nrow; i++) {
            mrp_free((char *)d->name);
            mrp_free((char *)d->type);

            v            = data->rows[i];
            d->name      = mrp_strdup(v[0].str);
            d->type      = mrp_strdup(v[1].str);
            d->public    = v[2].s32;
            d->available = v[3].s32;

            d += 1;
        }
    }

    list_devices();
}


static void init_streams(void)
{
    mrp_clear(&streams);
}


static void reset_streams(void)
{
    int i;

    for (i = 0; i < (int)MRP_ARRAY_SIZE(streams); i++) {
        mrp_free((char *)streams[i].name);
        mrp_free((char *)streams[i].role);
    }

    mrp_clear(&streams);
}


void update_streams(mrp_domctl_data_t *data)
{
    stream_t           *s;
    mrp_domctl_value_t *v;
    int                 i;

    if (data->nrow != 0 && data->ncolumn != STREAM_NCOLUMN) {
        error_msg("incorrect number of columns in stream update (%d != %d)",
                  data->ncolumn, STREAM_NCOLUMN);
        return;
    }

    if (data->nrow > (int)NSTREAM) {
        error_msg("too many rows (%d) in stream update", data->nrow);
        return;
    }

    if (data->nrow == 0)
        reset_streams();
    else {
        s = streams;

        for (i = 0; i < data->nrow; i++) {
            mrp_free((char *)s->name);
            mrp_free((char *)s->role);

            v          = data->rows[i];
            s->name    = mrp_strdup(v[0].str);
            s->role    = mrp_strdup(v[1].str);
            s->owner   = v[2].u32;
            s->playing = v[3].s32;

            s += 1;
        }
    }

    list_streams();
}


static void init_zones(void)
{
    mrp_clear(&zones);
}


static void reset_zones(void)
{
    int i;

    for (i = 0; i < (int)MRP_ARRAY_SIZE(zones); i++)
        mrp_free((char *)zones[i].name);

    mrp_clear(&zones);
}


void update_zones(mrp_domctl_data_t *data)
{
    zone_t             *z;
    mrp_domctl_value_t *v;
    int                 i;

    if (data->nrow != 0 && data->ncolumn != ZONE_NCOLUMN) {
        error_msg("incorrect number of columns in zone update (%d != %d)",
                  data->ncolumn, ZONE_NCOLUMN);
        return;
    }

    if (data->nrow > (int)NZONE) {
        error_msg("too many rows (%d) in zone update", data->nrow);
        return;
    }

    if (data->nrow == 0)
        reset_zones();
    else {
        z = zones;

        for (i = 0; i < data->nrow; i++) {
            mrp_free((char *)z->name);

            v           = data->rows[i];
            z->name     = mrp_strdup(v[0].str);
            z->occupied = v[1].s32;
            z->active   = v[2].s32;

            z += 1;
        }
    }

    list_zones();
}


static void init_calls(void)
{
    mrp_clear(&calls);
}


static void reset_calls(void)
{
    int i;

    for (i = 0; i < (int)MRP_ARRAY_SIZE(calls); i++) {
        mrp_free((char *)calls[i].state);
        mrp_free((char *)calls[i].modem);
    }

    mrp_clear(&calls);
}


void update_calls(mrp_domctl_data_t *data)
{
    call_t             *c;
    mrp_domctl_value_t *v;
    int                 i;

    if (data->nrow != 0 && data->ncolumn != CALL_NCOLUMN) {
        error_msg("incorrect number of columns in call update (%d != %d)",
                  data->ncolumn, CALL_NCOLUMN);
        return;
    }

    if (data->nrow > (int)NCALL) {
        error_msg("too many rows (%d) in call update", data->nrow);
        return;
    }

    if (data->nrow == 0)
        reset_calls();
    else {
        c = calls;

        for (i = 0; i < data->nrow; i++) {
            mrp_free((char *)c->state);
            mrp_free((char *)c->modem);

            v        = data->rows[i];
            c->id    = v[0].s32;
            c->state = mrp_strdup(v[1].str);
            c->modem = mrp_strdup(v[2].str);

            c += 1;
        }
    }

    list_calls();
}


void update_imports(client_t *c, mrp_domctl_data_t *data, int ntable)
{
    int i;

    MRP_UNUSED(ntable);

    for (i = 0; i < 2; i++) {
        if (c->zone) {
            if (data[i].id == 0)
                update_devices(data + i);
            else
                update_streams(data + i);
        }
        else {
            if (data[i].id == 0)
                update_zones(data + i);
            else
                update_calls(data + i);
        }
    }
}


static int ping_cb(mrp_domctl_t *dc, uint32_t narg, mrp_domctl_arg_t *args,
                   uint32_t *nout, mrp_domctl_arg_t *outs, void *user_data)
{
    client_t *c = (client_t *)user_data;
    int       i;

    MRP_UNUSED(dc);
    MRP_UNUSED(c);

    info_msg("pinged with %d arguments", narg);

    for (i = 0; i < (int)narg; i++) {
        switch (args[i].type) {
        case MRP_DOMCTL_STRING:
            info_msg("    #%d: %s", i, args[i].str);
            break;
        case MRP_DOMCTL_UINT32:
            info_msg("    #%d: %u", i, args[i].u32);
            break;
        default:
            if (MRP_DOMCTL_IS_ARRAY(args[i].type)) {
                uint32_t j;

                info_msg("    #%d: array of %u items:", i, args[i].size);
                for (j = 0; j < args[i].size; j++) {
                    switch (MRP_DOMCTL_ARRAY_TYPE(args[i].type)) {
                    case MRP_DOMCTL_STRING:
                        info_msg("        #%d: '%s'", j,
                                 ((char **)args[i].arr)[j]);
                        break;
                    case MRP_DOMCTL_UINT32:
                        info_msg("        #%d: %u", j,
                                 ((uint32_t *)args[i].arr)[j]);
                        break;
                    default:
                        info_msg("        #%d: <type 0x%x", j,
                                 MRP_DOMCTL_ARRAY_TYPE(args[i].type));
                        break;
                    }
                }
            }
            else
                info_msg("    <type 0x%x>", args[i].type);
        }
    }


    for (i = 0; i < (int)*nout; i++) {
        if (i < (int)narg) {
            if (MRP_DOMCTL_IS_ARRAY(args[i].type)) {
                int j;

                if (i & 0x1) {
                    outs[i].type = MRP_DOMCTL_ARRAY(STRING);
                    outs[i].arr  = mrp_allocz(sizeof(char *) * 5);
                    for (j = 0; j < 5; j++) {
                        char entry[32];
                        snprintf(entry, sizeof(entry), "xyzzy #%d.%d", i, j);
                        ((char **)outs[i].arr)[j] = mrp_strdup(entry);
                    }
                    outs[i].size = 5;
                }
                else {
                    outs[i].type = MRP_DOMCTL_ARRAY(UINT32);
                    outs[i].arr  = mrp_allocz(sizeof(uint32_t) * 5);
                    for (j = 0; j < 5; j++)
                        ((uint32_t*)outs[i].arr)[j] = 3141 + i * j;
                    outs[i].size = 5;
                }
            }
            else {
                outs[i] = args[i];

                if (outs[i].type == MRP_DOMCTL_STRING)
                    outs[i].str = mrp_strdup(outs[i].str);
            }
        }
        else {
            outs[i].type = MRP_DOMCTL_UINT32;
            outs[i].u32  = i;
        }
    }

    return 0;
}


void init_methods(client_t *c)
{
    mrp_domctl_method_def_t methods[] = {
        { "ping", 32, ping_cb, c },
    };
    int nmethod = MRP_ARRAY_SIZE(methods);

    mrp_domctl_register_methods(c->dc, methods, nmethod);
}


static void show_help(void)
{
#define P info_msg

    P("Available commands:");
    P("  help                                  show this help");
    P("  list                                  list all data");
    P("  list {devices|streams|zones|calls}    list the requested data");
    P("  plug <device>                         update <device> as plugged");
    P("  unplug <device>                       update <device> as unplugged");
    P("  play <stream>                         update <stream> as playing");
    P("  stop <stream>                         update <stream> as stopped");
    P("  call <call> <state>                   update state of <call>");
    P("  zone <zone> [occupied,[active]]       update state of <zone>");

#undef P
}


static void input_cb(brl_t *brl, const char *input, void *user_data)
{
    int len;

    MRP_UNUSED(user_data);

    brl_add_history(brl, input);

    if (input == NULL || !strcmp(input, "exit")) {
        brl_destroy(brl);
        exit(0);
    }
    else if (!strcmp(input, "help")) {
        show_help();
    }
    else if (!strcmp(input, "list")) {
        list_devices();
        list_streams();
        list_zones();
        list_calls();
    }
    else if (!strcmp(input, "list devices"))
        list_devices();
    else if (!strcmp(input, "list streams"))
        list_streams();
    else if (!strcmp(input, "list zones"))
        list_zones();
    else if (!strcmp(input, "list calls"))
        list_calls();
    else if (!strncmp(input, "plug "  , len=sizeof("plug ")   - 1) ||
             !strncmp(input, "unplug ", len=sizeof("unplug ") - 1)) {
        plug_device(client, input + len, *input == 'p');
    }
    else if (!strncmp(input, "play "  , len=sizeof("play ")   - 1) ||
             !strncmp(input, "stop ", len=sizeof("stop ") - 1)) {
        play_stream(client, input + len, *input == 'p');
    }
    else if (!strncmp(input, "call "  , len=sizeof("call ")   - 1)) {
        set_call_state(client, input + len);
    }
    else if (!strncmp(input, "zone "  , len=sizeof("zone ")   - 1)) {
        set_zone_state(client, input + len);
    }
}


static void terminal_setup(client_t *c)
{
    int         fd;
    const char *prompt;

    fd     = fileno(stdin);
    prompt = DEFAULT_PROMPT;
    c->brl = brl_create_with_murphy(fd, prompt, c->ml, input_cb, c);

    if (c->brl != NULL) {
        brl_show_prompt(c->brl);
    }
    else {
        mrp_log_error("Failed to breedline for console input.");
        exit(1);
    }
}


static void terminal_cleanup(client_t *c)
{
    if (c->brl != NULL) {
        brl_destroy(c->brl);
        c->brl = NULL;
    }
}


static void fatal_msg(int error, const char *format, ...)
{
    va_list ap;

    if (client && client->brl)
        brl_hide_prompt(client->brl);

    fprintf(stderr, "fatal error: ");
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);

    exit(error);
}


static void error_msg(const char *format, ...)
{
    va_list ap;

    if (client && client->brl)
        brl_hide_prompt(client->brl);

    fprintf(stderr, "error: ");
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);

    if (client && client->brl)
        brl_show_prompt(client->brl);
}


static void info_msg(const char *format, ...)
{
    va_list ap;

    if (client && client->brl)
        brl_hide_prompt(client->brl);

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);

    if (client && client->brl)
        brl_show_prompt(client->brl);
}


static void signal_handler(mrp_sighandler_t *h, int signum, void *user_data)
{
    mrp_mainloop_t *ml = mrp_get_sighandler_mainloop(h);

    MRP_UNUSED(user_data);

    switch (signum) {
    case SIGINT:
        info_msg("Got SIGINT, stopping...");
        if (ml != NULL)
            mrp_mainloop_quit(ml, 0);
        else
            exit(0);
        break;
    }
}


static void connect_notify(mrp_domctl_t *dc, int connected, int errcode,
                           const char *errmsg, void *user_data)
{
    MRP_UNUSED(dc);
    MRP_UNUSED(user_data);

    if (connected) {
        info_msg("Successfully registered to server.");
        export_data(client);
    }
    else
        error_msg("No connection to server (%d: %s).", errcode, errmsg);
}


static void dump_data(mrp_domctl_data_t *table)
{
    mrp_domctl_value_t *row;
    int                 i, j;
    char                buf[1024], *p;
    const char         *t;
    int                 n, l;

    info_msg("Table #%d: %d rows x %d columns", table->id,
             table->nrow, table->ncolumn);

    for (i = 0; i < table->nrow; i++) {
        row = table->rows[i];
        p   = buf;
        n   = sizeof(buf);

        for (j = 0, t = ""; j < table->ncolumn; j++, t = ", ") {
            switch (row[j].type) {
            case MRP_DOMCTL_STRING:
                l  = snprintf(p, n, "%s'%s'", t, row[j].str);
                p += l;
                n -= l;
                break;
            case MRP_DOMCTL_INTEGER:
                l  = snprintf(p, n, "%s%d", t, row[j].s32);
                p += l;
                n -= l;
                break;
            case MRP_DOMCTL_UNSIGNED:
                l  = snprintf(p, n, "%s%u", t, row[j].u32);
                p += l;
                n -= l;
                break;
            case MRP_DOMCTL_DOUBLE:
                l  = snprintf(p, n, "%s%f", t, row[j].dbl);
                p += l;
                n -= l;
                break;
            default:
                l  = snprintf(p, n, "%s<invalid column 0x%x>",
                              t, row[j].type);
                p += l;
                n -= l;
            }
        }

        info_msg("row #%d: { %s }", i, buf);
    }
}


static void data_notify(mrp_domctl_t *dc, mrp_domctl_data_t *tables,
                        int ntable, void *user_data)
{
    client_t *client = (client_t *)user_data;

    MRP_UNUSED(dc);

    if (client->verbose) {
        int i;

        for (i = 0; i < ntable; i++) {
            dump_data(tables + i);
        }
    }

    update_imports(client, tables, ntable);
}


static void export_notify(mrp_domctl_t *dc, int errcode, const char *errmsg,
                          void *user_data)
{
    MRP_UNUSED(dc);
    MRP_UNUSED(user_data);

    if (errcode != 0) {
        error_msg("Data set request failed (%d: %s).", errcode, errmsg);
    }
    else
        info_msg("Sucessfully set data.");
}


static void export_data(client_t *c)
{
    mrp_domctl_data_t  *tables;
    int                 ntable = 2;
    mrp_domctl_value_t *values, *v;
    int                 i, id;

    tables = alloca(sizeof(*tables) * ntable);
    values = alloca(sizeof(*values) * NVALUE);
    v      = values;

    if (!c->zone) {
        id = 0;

        tables[id].id      = id;
        tables[id].ncolumn = 4;
        tables[id].nrow    = NDEVICE;
        tables[id].rows    = alloca(sizeof(*tables[id].rows) * tables[id].nrow);

        for (i = 0; i < (int)NDEVICE; i++) {
            tables[id].rows[i] = v;
            v[0].type = MRP_DOMCTL_STRING ; v[0].str = devices[i].name;
            v[1].type = MRP_DOMCTL_STRING ; v[1].str = devices[i].type;
            v[2].type = MRP_DOMCTL_INTEGER; v[2].s32 = devices[i].public;
            v[3].type = MRP_DOMCTL_INTEGER; v[3].s32 = devices[i].available;
            v += 4;
        }

        id++;

        tables[id].id      = id;
        tables[id].ncolumn = 4;
        tables[id].nrow    = NSTREAM;
        tables[id].rows    = alloca(sizeof(*tables[id].rows) * tables[id].nrow);

        for (i = 0; i < (int)NSTREAM; i++) {
            tables[id].rows[i] = v;
            v[0].type = MRP_DOMCTL_STRING  ; v[0].str = streams[i].name;
            v[1].type = MRP_DOMCTL_STRING  ; v[1].str = streams[i].role;
            v[2].type = MRP_DOMCTL_UNSIGNED; v[2].s32 = streams[i].owner;
            v[3].type = MRP_DOMCTL_INTEGER ; v[3].u32 = streams[i].playing;
            v += 4;
        }
    }
    else {
        id = 0;

        tables[id].id      = id;
        tables[id].ncolumn = 3;
        tables[id].nrow    = NZONE;
        tables[id].rows    = alloca(sizeof(*tables[id].rows) * tables[id].nrow);

        for (i = 0; i < (int)NZONE; i++) {
            tables[id].rows[i] = v;
            v[0].type = MRP_DOMCTL_STRING ; v[0].str = zones[i].name;
            v[1].type = MRP_DOMCTL_INTEGER; v[1].s32 = zones[i].occupied;
            v[2].type = MRP_DOMCTL_INTEGER; v[2].s32 = zones[i].active;
            v += 3;
        }

        id++;

        tables[id].id      = id;
        tables[id].ncolumn = 3;
        tables[id].nrow    = NCALL;
        tables[id].rows    = alloca(sizeof(*tables[0].rows) * tables[id].nrow);

        for (i = 0; i < (int)NCALL; i++) {
            tables[id].rows[i] = v;
            v[0].type = MRP_DOMCTL_INTEGER; v[0].s32 = calls[i].id;
            v[1].type = MRP_DOMCTL_STRING ; v[1].str = calls[i].state;
            v[2].type = MRP_DOMCTL_STRING ; v[2].str = calls[i].modem;
            v += 3;
        }
    }

    if (!mrp_domctl_set_data(c->dc, tables, ntable, export_notify, c))
        error_msg("Failed to send data set request to server.");
}


static void client_setup(client_t *c)
{
    mrp_mainloop_t *ml;
    mrp_domctl_t   *dc;

    ml = mrp_mainloop_create();

    if (ml != NULL) {
        if (!c->zone) {
            exports = media_tables;
            nexport = MRP_ARRAY_SIZE(media_tables);
            imports = zone_watches;
            nimport = MRP_ARRAY_SIZE(zone_watches) - (c->audio ? 0 : 2);
        }
        else {
            exports = zone_tables;
            nexport = MRP_ARRAY_SIZE(zone_tables);
            imports = media_watches;
            nimport = MRP_ARRAY_SIZE(media_watches) - (c->audio ? 0 : 2);
        }

        if (c->audio)
            info_msg("Will subscribe for audio_playback_* tables.");

        dc = mrp_domctl_create(c->zone ? "zone-ctrl" : "media-ctrl", ml,
                               exports, nexport, imports, nimport,
                               connect_notify, data_notify, c);

        if (dc != NULL) {
            c->ml = ml;
            c->dc = dc;

            mrp_add_sighandler(ml, SIGINT, signal_handler, c);

            if (c->zone) {
                zone_t *z;
                call_t *call;

                for (z = zones; z->name != NULL; z++) {
                    z->name = mrp_strdup(z->name);
                }

                for (call = calls; call->id > 0; call++) {
                    call->state = mrp_strdup(call->state);
                    call->modem = mrp_strdup(call->modem);
                }

                init_devices();
                init_streams();
            }
            else {
                device_t *d;
                stream_t *s;

                for (d = devices; d->name != NULL; d++) {
                    d->name = mrp_strdup(d->name);
                    d->type = mrp_strdup(d->type);
                }

                for (s = streams; s->name != NULL; s++) {
                    s->name = mrp_strdup(s->name);
                    s->role = mrp_strdup(s->role);
                }

                init_zones();
                init_calls();
            }
        }
        else
            fatal_msg(1, "Failed to create enforcement point.");
    }
    else
        fatal_msg(1, "Failed to create mainloop.");

    init_methods(c);
}


static void client_cleanup(client_t *c)
{
    if (c->zone) {
        reset_devices();
        reset_streams();
    }
    else {
        reset_zones();
        reset_calls();
    }

    mrp_mainloop_destroy(c->ml);
    mrp_domctl_destroy(c->dc);

    c->ml = NULL;
    c->dc = NULL;
}


static void client_run(client_t *c)
{
    if (mrp_domctl_connect(c->dc, c->addrstr, 0))
        info_msg("Trying to connect to server at %s...", c->addrstr);
    else
        error_msg("Failed to connect to server at %s.", c->addrstr);

    mrp_mainloop_run(c->ml);
}


static void print_usage(const char *argv0, int exit_code, const char *fmt, ...)
{
    va_list ap;

    if (fmt && *fmt) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }

    printf("usage: %s [options]\n\n"
           "The possible options are:\n"
           "  -s, --server <address>     connect to murphy at given address\n"
           "  -z, --zone                 run as zone controller\n"
           "  -A, --audio                subscribe for audio_playback*\n"
           "  -v, --verbose              run in verbose mode\n"
           "  -h, --help                 show this help on usage\n",
           argv0);

    if (exit_code < 0)
        return;
    else
        exit(exit_code);
}


static void client_set_defaults(client_t *c)
{
    mrp_clear(c);
    c->addrstr = MRP_DEFAULT_DOMCTL_ADDRESS;
    c->zone    = FALSE;
    c->verbose = FALSE;
    c->audio   = FALSE;
}


int parse_cmdline(client_t *c, int argc, char **argv)
{
#   define OPTIONS "vAzhs:"
    struct option options[] = {
        { "zone"      , no_argument      , NULL, 'z' },
        { "verbose"   , optional_argument, NULL, 'v' },
        { "audio"     , no_argument      , NULL, 'A' },
        { "server"    , required_argument, NULL, 's' },
        { "help"      , no_argument      , NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;

    client_set_defaults(c);

    while ((opt = getopt_long(argc, argv, OPTIONS, options, NULL)) != -1) {
        switch (opt) {
        case 'z':
            c->zone = TRUE;
            break;

        case 'A':
            c->audio = TRUE;
            c->verbose = TRUE;
            break;

        case 'v':
            c->verbose = TRUE;
            break;

        case 's':
            c->addrstr = optarg;
            break;

        case 'h':
            print_usage(argv[0], -1, "");
            exit(0);
            break;

        default:
            print_usage(argv[0], EINVAL, "invalid option '%c'", opt);
        }
    }

    return TRUE;
}


int main(int argc, char *argv[])
{
    client_t  c;

    client_set_defaults(&c);
    parse_cmdline(&c, argc, argv);

    client_setup(&c);
    terminal_setup(&c);

    client = &c;
    client_run(&c);

    terminal_cleanup(&c);
    client_cleanup(&c);

    return 0;
}
