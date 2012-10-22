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

#include <readline/readline.h>
#include <readline/history.h>

#include <murphy/common.h>
#include "client.h"

#define DEFAULT_PROMPT "test-controller> "


/*
 * client context
 */

typedef struct {
    const char      *addrstr;            /* server address */
    int              zone;               /* run in zone control mode */
    int              verbose;            /* verbose mode */
    mrp_mainloop_t  *ml;                 /* murphy mainloop */
    void            *dc;                 /* domain controller */
    int              fd;                 /* fd for terminal input */
    mrp_io_watch_t  *iow;                /* I/O watch for terminal input */
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

mrp_domctl_table_t media_tables[] = {
    MRP_DOMCTL_TABLE("devices", DEVICE_COLUMNS, DEVICE_INDEX),
    MRP_DOMCTL_TABLE("streams", STREAM_COLUMNS, STREAM_INDEX),
};

mrp_domctl_watch_t media_watches[] = {
    MRP_DOMCTL_WATCH("devices", DEVICE_SELECT, DEVICE_WHERE, 0),
    MRP_DOMCTL_WATCH("streams", STREAM_SELECT, STREAM_WHERE, 0),
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
    MRP_DOMCTL_TABLE("zones", ZONE_COLUMNS, ZONE_INDEX),
    MRP_DOMCTL_TABLE("calls", CALL_COLUMNS, CALL_INDEX),
};

mrp_domctl_watch_t zone_watches[] = {
    MRP_DOMCTL_WATCH("zones", ZONE_SELECT, ZONE_WHERE, 0),
    MRP_DOMCTL_WATCH("calls", CALL_SELECT, CALL_WHERE, 0)
};

mrp_domctl_table_t *exports;
int                 nexport;
mrp_domctl_watch_t *imports;
int                 nimport;


static client_t *client;


static void fatal_msg(int error, const char *format, ...);
static void error_msg(const char *format, ...);
static void info_msg(const char *format, ...);

static void terminal_input_cb(char *input);

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


static void set_zone_state(client_t *c, char *config)
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


static void reset_devices(void)
{
    mrp_clear(&devices);
}


void update_devices(mrp_domctl_data_t *data)
{
    device_t           *d;
    mrp_domctl_value_t *v;
    int                 i;

    if (data->nrow != 0 && data->ncolumn != DEVICE_NCOLUMN) {
        error_msg("incorrect number of columns (%d) in device update",
                  data->ncolumn);
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


static void reset_streams(void)
{
    mrp_clear(&streams);
}


void update_streams(mrp_domctl_data_t *data)
{
    stream_t           *s;
    mrp_domctl_value_t *v;
    int                 i;

    if (data->nrow != 0 && data->ncolumn != STREAM_NCOLUMN) {
        error_msg("incorrect number of columns (%d) in stream update",
                  data->ncolumn);
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


static void reset_zones(void)
{
    mrp_clear(&zones);
}


void update_zones(mrp_domctl_data_t *data)
{
    zone_t             *z;
    mrp_domctl_value_t *v;
    int                 i;

    if (data->nrow != 0 && data->ncolumn != ZONE_NCOLUMN) {
        error_msg("incorrect number of columns (%d) in zone update",
                  data->ncolumn);
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


static void reset_calls(void)
{
    mrp_clear(&calls);
}


void update_calls(mrp_domctl_data_t *data)
{
    call_t             *c;
    mrp_domctl_value_t *v;
    int                 i;

    if (data->nrow != 0 && data->ncolumn != CALL_NCOLUMN) {
        error_msg("incorrect number of columns (%d) in call update.",
               data->ncolumn);
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

    for (i = 0; i < ntable; i++) {
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



static void terminal_prompt_erase(void)
{
    int n = strlen(DEFAULT_PROMPT);

    printf("\r");
    while (n-- > 0)
        printf(" ");
    printf("\r");
}


static void terminal_prompt_display(void)
{
    rl_callback_handler_remove();
    rl_callback_handler_install(DEFAULT_PROMPT, terminal_input_cb);
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


static void terminal_process_input(char *input)
{
    int len;

    add_history(input);

    if (input == NULL || !strcmp(input, "exit")) {
        terminal_prompt_erase();
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


static void terminal_input_cb(char *input)
{
    terminal_process_input(input);
    free(input);
}


static void terminal_cb(mrp_mainloop_t *ml, mrp_io_watch_t *w, int fd,
                        mrp_io_event_t events, void *user_data)
{
    MRP_UNUSED(w);
    MRP_UNUSED(fd);
    MRP_UNUSED(user_data);

    if (events & MRP_IO_EVENT_IN)
        rl_callback_read_char();

    if (events & MRP_IO_EVENT_HUP)
        mrp_mainloop_quit(ml, 0);
}


static void terminal_setup(client_t *c)
{
    mrp_io_event_t events;

    c->fd  = fileno(stdin);
    events = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP;
    c->iow = mrp_add_io_watch(c->ml, c->fd, events, terminal_cb, c);

    if (c->iow == NULL)
        fatal_msg(1, "Failed to create terminal input I/O watch.");
    else
        terminal_prompt_display();
}


static void terminal_cleanup(client_t *c)
{
    mrp_del_io_watch(c->iow);
    c->iow = NULL;

    rl_callback_handler_remove();
}


static void fatal_msg(int error, const char *format, ...)
{
    va_list ap;

    terminal_prompt_erase();

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

    terminal_prompt_erase();

    fprintf(stderr, "error: ");
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);

    terminal_prompt_display();
}


static void info_msg(const char *format, ...)
{
    va_list ap;

    terminal_prompt_erase();

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);

    terminal_prompt_display();
}


static void signal_handler(mrp_mainloop_t *ml, mrp_sighandler_t *h,
                           int signum, void *user_data)
{
    MRP_UNUSED(h);
    MRP_UNUSED(user_data);

    switch (signum) {
    case SIGINT:
        info_msg("Got SIGINT, stopping...");
        mrp_mainloop_quit(ml, 0);
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


static void data_notify(mrp_domctl_t *dc, mrp_domctl_data_t *tables,
                        int ntable, void *user_data)
{
    client_t *client = (client_t *)user_data;

    MRP_UNUSED(dc);

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
    int                 i;

    tables = alloca(sizeof(*tables) * ntable);
    values = alloca(sizeof(*values) * NVALUE);
    v      = values;

    if (!c->zone) {
        tables[0].id      = 0;
        tables[0].ncolumn = 4;
        tables[0].nrow    = NDEVICE;
        tables[0].rows    = alloca(sizeof(*tables[0].rows) * tables[0].nrow);

        for (i = 0; i < (int)NDEVICE; i++) {
            tables[0].rows[i] = v;
            v[0].type = MRP_DOMCTL_STRING ; v[0].str = devices[i].name;
            v[1].type = MRP_DOMCTL_STRING ; v[1].str = devices[i].type;
            v[2].type = MRP_DOMCTL_INTEGER; v[2].s32 = devices[i].public;
            v[3].type = MRP_DOMCTL_INTEGER; v[3].s32 = devices[i].available;
            v += 4;
        }

        tables[1].id      = 1;
        tables[1].ncolumn = 4;
        tables[1].nrow    = NSTREAM;
        tables[1].rows    = alloca(sizeof(*tables[1].rows) * tables[1].nrow);

        for (i = 0; i < (int)NSTREAM; i++) {
            tables[1].rows[i] = v;
            v[0].type = MRP_DOMCTL_STRING  ; v[0].str = streams[i].name;
            v[1].type = MRP_DOMCTL_STRING  ; v[1].str = streams[i].role;
            v[2].type = MRP_DOMCTL_UNSIGNED; v[2].s32 = streams[i].owner;
            v[3].type = MRP_DOMCTL_INTEGER ; v[3].u32 = streams[i].playing;
            v += 4;
        }
    }
    else {
        tables[0].id      = 0;
        tables[0].ncolumn = 3;
        tables[0].nrow    = NZONE;
        tables[0].rows    = alloca(sizeof(*tables[0].rows) * tables[0].nrow);

        for (i = 0; i < (int)NZONE; i++) {
            tables[0].rows[i] = v;
            v[0].type = MRP_DOMCTL_STRING ; v[0].str = zones[i].name;
            v[1].type = MRP_DOMCTL_INTEGER; v[1].s32 = zones[i].occupied;
            v[2].type = MRP_DOMCTL_INTEGER; v[2].s32 = zones[i].active;
            v += 3;
        }

        tables[1].id      = 1;
        tables[1].ncolumn = 3;
        tables[1].nrow    = NCALL;
        tables[1].rows    = alloca(sizeof(*tables[0].rows) * tables[1].nrow);

        for (i = 0; i < (int)NCALL; i++) {
            tables[1].rows[i] = v;
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
            nimport = MRP_ARRAY_SIZE(zone_watches);
        }
        else {
            exports = zone_tables;
            nexport = MRP_ARRAY_SIZE(zone_tables);
            imports = media_watches;
            nimport = MRP_ARRAY_SIZE(media_watches);
        }

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

                reset_devices();
                reset_streams();
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

                reset_zones();
                reset_calls();
            }
        }
        else
            fatal_msg(1, "Failed to create enforcement point.");
    }
    else
        fatal_msg(1, "Failed to create mainloop.");
}


static void client_cleanup(client_t *c)
{
    mrp_mainloop_destroy(c->ml);
    mrp_domctl_destroy(c->dc);

    c->ml = NULL;
    c->dc = NULL;
}


static void client_run(client_t *c)
{
    if (mrp_domctl_connect(c->dc, c->addrstr))
        info_msg("Connected to server at %s.", c->addrstr);
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
}


int parse_cmdline(client_t *c, int argc, char **argv)
{
#   define OPTIONS "vzhs:"
    struct option options[] = {
        { "server"    , required_argument, NULL, 's' },
        { "zone"      , no_argument      , NULL, 'z' },
        { "verbose"   , optional_argument, NULL, 'v' },
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

        case 'v':
            c->verbose = TRUE;
            break;

        case 'a':
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
