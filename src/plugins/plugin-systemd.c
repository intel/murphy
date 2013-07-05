#include <stdlib.h>
#include <syslog.h>

#include <systemd/sd-daemon.h>
#include <systemd/sd-journal.h>

#include <murphy/common.h>
#include <murphy/core.h>

static void sdlogger(void *data, mrp_log_level_t level, const char *file,
                     int line, const char *func, const char *format, va_list ap)
{
    va_list cp;
    int     prio;
    char    filebuf[1024], linebuf[64];

    MRP_UNUSED(data);

    va_copy(cp, ap);
    switch (level) {
    case MRP_LOG_ERROR:   prio = LOG_ERR;     break;
    case MRP_LOG_WARNING: prio = LOG_WARNING; break;
    case MRP_LOG_INFO:    prio = LOG_INFO;    break;
    case MRP_LOG_DEBUG:   prio = LOG_DEBUG;   break;
    default:              prio = LOG_INFO;
    }

    snprintf(filebuf, sizeof(filebuf), "CODE_FILE=%s", file);
    snprintf(linebuf, sizeof(linebuf), "CODE_LINE=%d", line);
    sd_journal_printv_with_location(prio, filebuf, linebuf, func, format, cp);

    va_end(cp);
}


static int sdlogger_init(mrp_plugin_t *plugin)
{
    MRP_UNUSED(plugin);

    if (mrp_log_register_target("systemd", sdlogger, NULL))
        mrp_log_info("systemd: registered logging target.");
    else
        mrp_log_error("systemd: failed to register logging target.");

    return TRUE;
}


static void sdlogger_exit(mrp_plugin_t *plugin)
{
    MRP_UNUSED(plugin);

    mrp_log_unregister_target("systemd");

    return;
}

#define SDLOGGER_DESCRIPTION "A systemd logger for Murphy."
#define SDLOGGER_HELP        "systemd logger support for Murphy."
#define SDLOGGER_VERSION     MRP_VERSION_INT(0, 0, 1)
#define SDLOGGER_AUTHORS     "Krisztian Litkey <kli@iki.fi>"

MURPHY_REGISTER_PLUGIN("systemd",
                       SDLOGGER_VERSION, SDLOGGER_DESCRIPTION,
                       SDLOGGER_AUTHORS, SDLOGGER_HELP, MRP_SINGLETON,
                       sdlogger_init, sdlogger_exit,
                       NULL, 0, NULL, 0, NULL, 0, NULL);
