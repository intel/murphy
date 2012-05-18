#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <murphy/common.h>


/*
 * tags for generic message fields
 */

#define TAG_SEQ   ((uint16_t)0x1)
#define TAG_MSG   ((uint16_t)0x2)
#define TAG_U8    ((uint16_t)0x3)
#define TAG_S8    ((uint16_t)0x4)
#define TAG_U16   ((uint16_t)0x5)
#define TAG_S16   ((uint16_t)0x6)
#define TAG_DBL   ((uint16_t)0x7)
#define TAG_BLN   ((uint16_t)0x8)
#define TAG_ASTR  ((uint16_t)0x9)
#define TAG_AU32  ((uint16_t)0xa)
#define TAG_RPL   ((uint16_t)0xb)
#define TAG_END   MRP_MSG_FIELD_END

#define U32_GUARD (uint32_t)-1

/*
 * our test custom data type
 */

#define TAG_CUSTOM 0x1

typedef struct {
    uint32_t   seq;
    char      *msg;
    uint8_t     u8;
    int8_t      s8;
    uint16_t   u16;
    int16_t    s16;
    double     dbl;
    bool       bln;
    char     **astr;
    uint32_t   nstr;
    uint32_t  *au32;
    char      *rpl;
} custom_t;


MRP_DATA_DESCRIPTOR(custom_descr, TAG_CUSTOM, custom_t,
		    MRP_DATA_MEMBER(custom_t,  seq, MRP_MSG_FIELD_UINT32),
		    MRP_DATA_MEMBER(custom_t,  msg, MRP_MSG_FIELD_STRING),
		    MRP_DATA_MEMBER(custom_t,   u8, MRP_MSG_FIELD_UINT8 ),
		    MRP_DATA_MEMBER(custom_t,   s8, MRP_MSG_FIELD_SINT8 ),
		    MRP_DATA_MEMBER(custom_t,  u16, MRP_MSG_FIELD_UINT16),
		    MRP_DATA_MEMBER(custom_t,  s16, MRP_MSG_FIELD_SINT16),
		    MRP_DATA_MEMBER(custom_t,  dbl, MRP_MSG_FIELD_DOUBLE),
		    MRP_DATA_MEMBER(custom_t,  bln, MRP_MSG_FIELD_BOOL  ),
		    MRP_DATA_MEMBER(custom_t,  rpl, MRP_MSG_FIELD_STRING),
		    MRP_DATA_MEMBER(custom_t, nstr, MRP_MSG_FIELD_UINT32),
		    MRP_DATA_ARRAY_COUNT(custom_t, astr, nstr,
					 MRP_MSG_FIELD_STRING),
		    MRP_DATA_ARRAY_GUARD(custom_t, au32, u32, U32_GUARD,
					 MRP_MSG_FIELD_UINT32));


typedef struct {
    mrp_mainloop_t  *ml;
    mrp_transport_t *lt, *t;
    char            *addr;
    int              server;
    int              sock;
    mrp_io_watch_t  *iow;
    mrp_timer_t     *timer;
    int              custom;
    int              log_mask;
    const char      *log_target;
    uint32_t         seqno;
} context_t;


void recv_custom(mrp_transport_t *t, void *data, uint16_t tag, void *user_data);
void recv_msg(mrp_transport_t *t, mrp_msg_t *msg, void *user_data);


void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    context_t *c = (context_t *)user_data;

    MRP_UNUSED(t);
    MRP_UNUSED(c);

    if (error) {
	mrp_log_error("Connection closed with error %d (%s).", error,
		      strerror(error));
	exit(1);
    }
    else {
	mrp_log_info("Peer has closed the connection.");
	exit(0);
    }
}


void dump_msg(mrp_msg_t *msg, FILE *fp)
{
    mrp_msg_dump(msg, fp);
}


void dump_custom(custom_t *msg, FILE *fp)
{
    uint32_t i;
    
    mrp_data_dump(msg, &custom_descr, fp);
    fprintf(fp, "{\n");
    fprintf(fp, "    seq = %u\n"  , msg->seq);
    fprintf(fp, "    msg = '%s'\n", msg->msg);
    fprintf(fp, "     u8 = %u\n"  , msg->u8);
    fprintf(fp, "     s8 = %d\n"  , msg->s8);
    fprintf(fp, "    u16 = %u\n"  , msg->u16);
    fprintf(fp, "    s16 = %d\n"  , msg->s16);
    fprintf(fp, "    dbl = %f\n"  , msg->dbl);
    fprintf(fp, "    bln = %s\n"  , msg->bln ? "true" : "false");
    fprintf(fp, "   astr = (%u)\n", msg->nstr);
    for (i = 0; i < msg->nstr; i++)
	fprintf(fp, "           %s\n", msg->astr[i]);
    fprintf(fp, "   au32 =\n");
    for (i = 0; msg->au32[i] != U32_GUARD; i++)
	fprintf(fp, "           %u\n", msg->au32[i]);
    fprintf(fp, "    rpl = '%s'\n", msg->rpl);
    fprintf(fp, "}\n");
}


void free_custom(custom_t *msg)
{
    mrp_data_free(msg, custom_descr.tag);
}


void recv_msg(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
{
    context_t       *c = (context_t *)user_data;
    mrp_msg_field_t *f;
    uint32_t         seq;
    char             buf[256];
    
    mrp_log_info("received a message");
    dump_msg(msg, stdout);
    
    if (c->server) {
	seq = 0;
	if ((f = mrp_msg_find(msg, TAG_SEQ)) != NULL) {
	    if (f->type == MRP_MSG_FIELD_UINT32)
		seq = f->u32;
	}
	    
	snprintf(buf, sizeof(buf), "reply to message #%u", seq);
	
	if (!mrp_msg_append(msg, TAG_RPL, MRP_MSG_FIELD_STRING, buf,
			    TAG_END)) {
	    mrp_log_info("failed to append to received message");
	    exit(1);
	}
			   
	if (mrp_transport_send(t, msg))
	    mrp_log_info("reply successfully sent");
	else
	    mrp_log_error("failed to send reply");

	/* message unreffed by transport layer */
    }
}


void recv_custom(mrp_transport_t *t, void *data, uint16_t tag, void *user_data)
{
    context_t *c   = (context_t *)user_data;
    custom_t  *msg = (custom_t *)data;
    custom_t   rpl;
    char       buf[256];
    uint32_t   au32[] = { 9, 8, 7, 6, 5, -1 };

    mrp_log_info("received custom message of type 0x%x", tag);
    dump_custom(data, stdout);

    if (tag != custom_descr.tag) {
	mrp_log_error("Tag 0x%x != our custom type (0x%x).",
		      tag, custom_descr.tag);
	exit(1);
    }
    
    
    if (c->server) {
	rpl = *msg;
	snprintf(buf, sizeof(buf), "reply to message #%u", msg->seq);
	rpl.rpl  = buf;
	rpl.au32 = au32;
	
	if (mrp_transport_senddata(t, &rpl, custom_descr.tag))
	    mrp_log_info("reply successfully sent");
	else
	    mrp_log_error("failed to send reply");
    }
    
    free_custom(msg);
}


void connection_evt(mrp_transport_t *lt, void *user_data)
{
    static mrp_transport_evt_t evt = {
	.closed   = closed_evt,
	.recv     = NULL,
	.recvfrom = NULL,
    };

    context_t *c = (context_t *)user_data;
    int        flags;

    if (c->custom)
	evt.recvdata = recv_custom;
    else
	evt.recv     = recv_msg;

    flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_NONBLOCK;
    c->t = mrp_transport_accept(lt, &evt, c, flags);

    if (c->t == NULL) {
	mrp_log_error("Failed to accept new connection.");
	exit(1);
    }
}


void type_init(void)
{
    if (!mrp_msg_register_type(&custom_descr)) {
	mrp_log_error("Failed to register custom data type.");
	exit(1);
    }
}

void server_init(context_t *c)
{
    static mrp_transport_evt_t evt = {
	.closed     = NULL,
	.recv       = NULL,
	.recvfrom   = NULL,
	.connection = connection_evt,
    };

    mrp_sockaddr_t addr;
    socklen_t      addrlen;
    const char    *type;
    int            flags;

    addrlen = mrp_transport_resolve(NULL, c->addr, &addr, sizeof(addr), &type);

    if (addrlen > 0) {
	type_init();
	
	flags = MRP_TRANSPORT_REUSEADDR | 
	    c->custom ? MRP_TRANSPORT_MODE_CUSTOM : 0;
	c->lt = mrp_transport_create(c->ml, type, &evt, c, flags);

	if (c->lt == NULL) {
	    mrp_log_error("Failed to create listening server transport.");
	    exit(1);
	}

	if (!mrp_transport_bind(c->lt, &addr, addrlen)) {
	    mrp_log_error("Failed to bind transport to address %s.", c->addr);
	    exit(1);
	}

	if (!mrp_transport_listen(c->lt, 0)) {
	    mrp_log_error("Failed to listen on server transport.");
	    exit(1);
	}
    }    
    else {
	mrp_log_error("Failed to resolve address %s.", c->addr);
	exit(1);
    }
}



void send_msg(context_t *c)
{
    mrp_msg_t *msg;
    uint32_t   seq;
    char       buf[256];
    char      *astr[] = { "this", "is", "an", "array", "of", "strings" };
    uint32_t   au32[] = { 1, 2, 3,
			  1 << 16, 2 << 16, 3 << 16,
			  1 << 24, 2 << 24, 3 << 24 };
    uint32_t   nstr = MRP_ARRAY_SIZE(astr);
    uint32_t   nu32 = MRP_ARRAY_SIZE(au32);

    seq = c->seqno++;
    snprintf(buf, sizeof(buf), "this is message #%u", (unsigned int)seq);
    
    msg = mrp_msg_create(TAG_SEQ , MRP_MSG_FIELD_UINT32, seq,
			 TAG_MSG , MRP_MSG_FIELD_STRING, buf,
			 TAG_U8  , MRP_MSG_FIELD_UINT8 ,   seq & 0xf,
			 TAG_S8  , MRP_MSG_FIELD_SINT8 , -(seq & 0xf),
			 TAG_U16 , MRP_MSG_FIELD_UINT16,   seq,
			 TAG_S16 , MRP_MSG_FIELD_SINT16, - seq,
			 TAG_DBL , MRP_MSG_FIELD_DOUBLE, seq / 3.0,
			 TAG_BLN , MRP_MSG_FIELD_BOOL  , seq & 0x1,
			 TAG_ASTR, MRP_MSG_FIELD_ARRAY_OF(STRING), nstr, astr,
			 TAG_AU32, MRP_MSG_FIELD_ARRAY_OF(UINT32), nu32, au32,
			 TAG_END);
    
    if (msg == NULL) {
	mrp_log_error("Failed to create new message.");
	exit(1);
    }

    if (!mrp_transport_send(c->t, msg)) {
	mrp_log_error("Failed to send message #%d.", seq);
	exit(1);
    }
    else
	mrp_log_info("Message #%d succesfully sent.", seq);
    
    mrp_msg_unref(msg);
}


void send_custom(context_t *c)
{
    uint32_t  seq = c->seqno++;
    custom_t  msg;
    char      buf[256];
    char     *astr[] = { "this", "is", "a", "test", "string", "array" };
    uint32_t  au32[] = { 1, 2, 3, 4, 5, 6, 7, -1 };

    msg.seq = seq;
    snprintf(buf, sizeof(buf), "this is message #%u", (unsigned int)seq);
    msg.msg  = buf;
    msg.u8   =   seq & 0xf;
    msg.s8   = -(seq & 0xf);
    msg.u16  =   seq;
    msg.s16  = - seq;
    msg.dbl  =   seq / 3.0;
    msg.bln  =   seq & 0x1;
    msg.astr = astr;
    msg.nstr = MRP_ARRAY_SIZE(astr);
    msg.au32 = au32;
    msg.rpl  = "";

    if (!mrp_transport_senddata(c->t, &msg, custom_descr.tag)) {
	mrp_log_error("Failed to send message #%d.", msg.seq);
	exit(1);
    }
    else
	mrp_log_info("Message #%d succesfully sent.", msg.seq);
}



void send_cb(mrp_mainloop_t *ml, mrp_timer_t *t, void *user_data)
{
    context_t *c = (context_t *)user_data;
    
    MRP_UNUSED(ml);
    MRP_UNUSED(t);

    if (c->custom)
	send_custom(c);
    else
	send_msg(c);
}


void client_init(context_t *c)
{
    static mrp_transport_evt_t evt = {
	.closed   = closed_evt,
	.recv     = NULL,
	.recvfrom = NULL,
    };

    mrp_sockaddr_t  addr;
    socklen_t       addrlen;
    const char     *type;
    int             flags;

    addrlen = mrp_transport_resolve(NULL, c->addr, &addr, sizeof(addr), &type);
    
    if (addrlen <= 0) {
	mrp_log_error("Failed resolve transport address '%s'.", c->addr);
	exit(1);
    }
    
    type_init();

    if (c->custom)
	evt.recvdata = recv_custom;
    else
	evt.recv     = recv_msg;

    flags = c->custom ? MRP_TRANSPORT_MODE_CUSTOM : 0;
    c->t  = mrp_transport_create(c->ml, type, &evt, c, flags);
    
    if (c->t == NULL) {
	mrp_log_error("Failed to create new transport.");
	exit(1);
    }

    if (!mrp_transport_connect(c->t, &addr, addrlen)) {
	mrp_log_error("Failed to connect to %s.", c->addr);
	exit(1);
    }
    
    c->timer = mrp_add_timer(c->ml, 1000, send_cb, c);
    
    if (c->timer == NULL) {
	mrp_log_error("Failed to create send timer.");
	exit(1);
    }
}


static void print_usage(const char *argv0, int exit_code, const char *fmt, ...)
{
    va_list ap;
    
    if (fmt && *fmt) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
    
    printf("usage: %s [options] [transport-address]\n\n"
           "The possible options are:\n"
           "  -s, --server                   run as test server (default)\n"
	   "  -c, --custom                   use custom messages\n"
	   "  -m, --message                  use generic messages (default)\n"
           "  -t, --log-target=TARGET        log target to use\n"
           "      TARGET is one of stderr,stdout,syslog, or a logfile path\n"
           "  -l, --log-level=LEVELS         logging level to use\n"
           "      LEVELS is a comma separated list of info, error and warning\n"
           "  -v, --verbose                  increase logging verbosity\n"
           "  -d, --debug                    enable debug messages\n"
           "  -h, --help                     show help on usage\n",
           argv0);
    
    if (exit_code < 0)
	return;
    else
	exit(exit_code);
}


static void config_set_defaults(context_t *ctx)
{
    mrp_clear(ctx);
    ctx->addr       = "tcp4:127.0.0.1:3000";
    ctx->server     = FALSE;
    ctx->custom     = FALSE;
    ctx->log_mask   = MRP_LOG_UPTO(MRP_LOG_DEBUG);
    ctx->log_target = MRP_LOG_TO_STDERR;
}


int parse_cmdline(context_t *ctx, int argc, char **argv)
{
    #define OPTIONS "scma:l:t:vdh"
    struct option options[] = {
	{ "server"    , no_argument      , NULL, 's' },
	{ "address"   , required_argument, NULL, 'a' },
	{ "custom"    , no_argument      , NULL, 'c' },
	{ "message"   , no_argument      , NULL, 'm' },
	{ "log-level" , required_argument, NULL, 'l' },
	{ "log-target", required_argument, NULL, 't' },
	{ "verbose"   , optional_argument, NULL, 'v' },
	{ "debug"     , no_argument      , NULL, 'd' },
	{ "help"      , no_argument      , NULL, 'h' },
	{ NULL, 0, NULL, 0 }
    };

    int  opt, debug;

    debug = FALSE;
    config_set_defaults(ctx);
    
    while ((opt = getopt_long(argc, argv, OPTIONS, options, NULL)) != -1) {
        switch (opt) {
        case 's':
	    ctx->server = TRUE;
	    break;

        case 'c':
	    ctx->custom = TRUE;
	    break;

	case 'm':
	    ctx->custom = FALSE;
	    break;

	case 'a':
	    ctx->addr = optarg;
	    break;

	case 'v':
	    ctx->log_mask <<= 1;
	    ctx->log_mask  |= 1;
	    break;

	case 'l':
	    ctx->log_mask = mrp_log_parse_levels(optarg);
	    if (ctx->log_mask < 0)
		print_usage(argv[0], EINVAL, "invalid log level '%s'", optarg);
	    break;

	case 't':
	    ctx->log_target = mrp_log_parse_target(optarg);
	    if (!ctx->log_target)
		print_usage(argv[0], EINVAL, "invalid log target '%s'", optarg);
	    break;

	case 'd':
	    debug = TRUE;
	    break;
	    
	case 'h':
	    print_usage(argv[0], -1, "");
	    exit(0);
	    break;

        default:
	    print_usage(argv[0], EINVAL, "invalid option '%c'", opt);
	}
    }

    if (debug)
	ctx->log_mask |= MRP_LOG_MASK_DEBUG;
    
    return TRUE;
}


int main(int argc, char *argv[])
{
    context_t c;

    mrp_clear(&c);

    if (!parse_cmdline(&c, argc, argv))
	exit(1);
    
    mrp_log_set_mask(c.log_mask);
    mrp_log_set_target(c.log_target);
    
    if (c.server)
	mrp_log_info("Running as server, using address '%s'...", c.addr);
    else
	mrp_log_info("Running as client, using address '%s'...", c.addr);
    
    if (c.custom)
	mrp_log_info("Using custom messages...");
    else
	mrp_log_info("Using generic messages...");

    c.ml = mrp_mainloop_create();

    if (c.server)
	server_init(&c);
    else
	client_init(&c);
    
    mrp_mainloop_run(c.ml);

    return 0;
}
