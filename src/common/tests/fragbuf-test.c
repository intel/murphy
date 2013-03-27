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

#include <errno.h>
#include <getopt.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/fragbuf.h>


#define fatal(fmt, args...) do {                \
        mrp_log_error(fmt, ## args);            \
        exit(1);                                \
    } while (0)


typedef struct {
    int         log_mask;
    const char *log_target;
    int         framed;
} context_t;

context_t ctx;

void check_message(void *data, size_t size, char **messages,
                   int *chk, int *offs)
{
    char *p, *d;
    int   l;

    if (ctx.framed) {
        if (!strncmp(messages[*chk], data, size) && !messages[*chk][size])
            mrp_debug("message check: OK");
        else
            fatal("message check: failed");

        *chk += 1;
    }
    else {
        d = data;
        while (size > 0) {
            p = messages[*chk] + *offs;
            l = strlen(p);

            if (l > (int)size)
                l = (int)size;

            if (strncmp(p, d, l))
                fatal("message check: failed");

            *offs += l;
            size  -= l;
            d     += l;

            if (messages[*chk][*offs] == '\0') {
                *chk  += 1;
                *offs  = 0;
            }
        }
        mrp_debug("message check: OK");
    }
}


void dump_buffer(mrp_fragbuf_t *buf, char **messages, int *chk, int *offs)
{
    void   *data;
    size_t  size;
    int     cnt;

    data = NULL;
    size = 0;
    cnt  = 0;

    while (mrp_fragbuf_pull(buf, &data, &size)) {
        mrp_log_info("got message: (%zd bytes) [%*.*s]", size,
                     (int)size, (int)size, (char *)data);

        check_message(data, size, messages, chk, offs);

        cnt++;
    }

    if (!cnt)
        mrp_debug("no full messages in buffer");
    else
        mrp_debug("pulled %d messages from buffer...", cnt);
}


int test(mrp_fragbuf_t *buf, size_t *chunks, int dump_interval)
{
    char *messages[] = {
        "Ticking away the moments",
        "That make up a dull day",
        "Fritter and waste the hours",
        "In an off-hand way",
        "Kicking around on a piece of ground",
        "In your home town",
        "Waiting for someone or something",
        "To show you the way",
        "Tired of lying in the sunshine",
        "Staying home to watch the rain",
        "You are young and life is long",
        "And there is time to kill today",
        "And then the one day you find",
        "Ten years have got behind you",
        "No one told you when to run",
        "You missed the starting gun",
        "And you run and you run",
        "To catch up with the sun",
        "But it's sinking",
        "Racing around",
        "To come up behind you again",
        "The sun is the same",
        "In a relative way",
        "But you're older",
        "Shorter of breath",
        "And one day closer to death",
        "Every year is getting shorter",
        "Never seem to find the time",
        "Plans that either come to naught",
        "Or half a page of scribbled lines",
        "Hanging on in quiet desperation",
        "Is the English way",
        "The time is gone",
        "The song is over",
        "Thought I'd something more to say",
        "Home",
        "Home again",
        "I like to be here",
        "When I can",
        "When I come home",
        "Cold and tired",
        "It's good to warm my bones",
        "Beside the fire",
        "Far away",
        "Across the field",
        "Tolling on the iron bell",
        "Calls the faithful to their knees",
        "To hear the softly spoken magic spell...",
        "test #1",
        "test #2",
        "this is a test #3",
        "message #4",
        "message #5",
        "test message #6",
        "a test #7",
        "the quick brown (#8)",
        "fox (#9)",
        "jumps over the (#10)",
        "lazy dog (#11)",
        "this is another test message (#12)",
        "and here is one more for you (#13)",
        "foo (#14)",
        "bar (#15)",
        "foobar (#16)",
        "barfoo (#17)",
        "xyzzykukkuluuruu (#18)"
    };

    char          *msg, *p;
    uint32_t       size, nbo_size;
    size_t         n, total;
    int            dump, chk, offs, i, j;

    dump = chk = offs = 0;

    for (i = 0; i < (int)MRP_ARRAY_SIZE(messages); i++) {
        msg   = messages[i];
        size  = strlen(msg);

        total = 0;
        p     = msg;

        if (ctx.framed) {
            nbo_size = htobe32(size);
            if (!mrp_fragbuf_push(buf, &nbo_size, sizeof(nbo_size)))
                fatal("failed to push message size to buffer");
        }

        for (j = 0; *p != '\0'; j++) {
            if (!chunks[j])
                j = 0;
            n = chunks[j];
            if (n > strlen(p))
                n = strlen(p);

            mrp_debug("pushing %zd bytes (%*.*s)...", n, (int)n, (int)n, p);

            if (!mrp_fragbuf_push(buf, p, n))
                fatal("failed to push %*.*s to buffer", (int)n, (int)n, p);

            p     += n;
            total += n;

            dump++;

            if (!dump_interval ||
                (dump_interval > 0 && !(dump % dump_interval)))
                dump_buffer(buf, messages, &chk, &offs);
        }

        if (dump_interval < -1) {
            if (i && !(i % -dump_interval))
                dump_buffer(buf, messages, &chk, &offs);
        }
    }

    dump_buffer(buf, messages, &chk, &offs);

    return TRUE;
}


static void print_usage(const char *argv0, int exit_code, const char *fmt, ...)
{
    va_list ap;

    if (fmt && *fmt) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        printf("\n");
        va_end(ap);
    }

    printf("usage: %s [options]\n\n"
           "The possible options are:\n"
           "  -t, --log-target=TARGET        log target to use\n"
           "      TARGET is one of stderr,stdout,syslog, or a logfile path\n"
           "  -l, --log-level=LEVELS         logging level to use\n"
           "      LEVELS is a comma separated list of info, error and warning\n"
           "  -v, --verbose                  increase logging verbosity\n"
           "  -d, --debug                    enable debug messages\n"
           "  -n, --non-framed               set buffer to non-framed mode\n"
           "  -h, --help                     show help on usage\n",
           argv0);

    if (exit_code < 0)
        return;
    else
        exit(exit_code);
}


static void config_set_defaults(void)
{
    mrp_clear(&ctx);
    ctx.log_mask   = MRP_LOG_UPTO(MRP_LOG_INFO);
    ctx.log_target = MRP_LOG_TO_STDOUT;
    ctx.framed     = TRUE;
}


void parse_cmdline(int argc, char **argv)
{
#   define OPTIONS "l:t:vd:nh"
    struct option options[] = {
        { "log-level" , required_argument, NULL, 'l' },
        { "log-target", required_argument, NULL, 't' },
        { "verbose"   , optional_argument, NULL, 'v' },
        { "debug"     , required_argument, NULL, 'd' },
        { "non-framed", no_argument      , NULL, 'n' },
        { "help"      , no_argument      , NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;

    config_set_defaults();

    while ((opt = getopt_long(argc, argv, OPTIONS, options, NULL)) != -1) {
        switch (opt) {
        case 'v':
            ctx.log_mask <<= 1;
            ctx.log_mask  |= 1;
            break;

        case 'l':
            ctx.log_mask = mrp_log_parse_levels(optarg);
            if (ctx.log_mask < 0)
                print_usage(argv[0], EINVAL, "invalid log level '%s'", optarg);
            break;

        case 't':
            ctx.log_target = mrp_log_parse_target(optarg);
            if (!ctx.log_target)
                print_usage(argv[0], EINVAL, "invalid log target '%s'", optarg);
            break;

        case 'd':
            ctx.log_mask |= MRP_LOG_MASK_DEBUG;
            mrp_debug_set_config(optarg);
            mrp_debug_enable(TRUE);
            break;

        case'n':
            ctx.framed = FALSE;
            break;

        case 'h':
            print_usage(argv[0], -1, "");
            exit(0);
            break;

        case '?':
            if (opterr)
                print_usage(argv[0], EINVAL, "");
            break;

        default:
            print_usage(argv[0], EINVAL, "invalid option '%c'", opt);
        }
    }
}

int main(int argc, char *argv[])
{
    mrp_fragbuf_t *buf;
    size_t         chunkstbl[][8] = {
        { 3, 1, 2, 3, 5, 0, 0, 0 },
        { 1, 2, 3, 4, 3, 2, 1, 0 },
        { 1, 5, 3, 4, 2, 1, 1, 0 },
        { 4, 3, 2, 1, 2, 3, 4, 0 },
    };
    size_t *chunks;
    size_t  single[]    = { 1, 0 };
    int     intervals[] = { 1, 2, 3, 4, 5, 0, -1 };
    int     i, j, interval;

    parse_cmdline(argc, argv);

    mrp_log_set_mask(ctx.log_mask);
    mrp_log_set_target(ctx.log_target);

    buf = mrp_fragbuf_create(ctx.framed, 0);

    if (buf == NULL)
        fatal("failed to create data collecting buffer");

    for (i = 0; i < (int)MRP_ARRAY_SIZE(intervals); i++) {
        interval = intervals[i];
        for (j = 0; j < (int)MRP_ARRAY_SIZE(chunkstbl); j++) {
            chunks = &chunkstbl[j][0];
            mrp_log_info("testing with interval %d, chunks #%d", interval, j);
            test(buf, chunks, interval);
            test(buf, single, interval);
            mrp_log_info("testing with interval %d, chunks #%d", -i -2, j);
            test(buf, chunks, -i - 2);
            test(buf, single, -i - 2);
        }
    }

    mrp_fragbuf_destroy(buf);

    return 0;
}
