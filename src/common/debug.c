#define _GNU_SOURCE
#include <link.h>
#include <elf.h>

#include <stdarg.h>
#include <limits.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/log.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/utils.h>
#include <murphy/common/debug.h>

#define WILDCARD  "*"

int mrp_debug_stamp = 0;     /* debug config stamp */

static int         debug_enabled;           /* debug messages enabled */
static mrp_htbl_t *rules_on;                /* enabling rules */
static mrp_htbl_t *rules_off;               /* disabling rules */
static mrp_htbl_t *files;                   /* table of per-file debug info */
static MRP_LIST_HOOK(debug_files);

static void populate_file_table(void);

static void free_rule_cb(void *key, void *entry)
{
    MRP_UNUSED(key);

    mrp_free(entry);
}


static int init_rules(void)
{
    mrp_htbl_config_t hcfg;

    mrp_clear(&hcfg);
    hcfg.comp = mrp_string_comp;
    hcfg.hash = mrp_string_hash;
    hcfg.free = free_rule_cb;

    rules_on  = mrp_htbl_create(&hcfg);
    rules_off = mrp_htbl_create(&hcfg);

    if (rules_on == NULL || rules_off == NULL)
        return FALSE;
    else
        return TRUE;
}


static void reset_rules(void)
{
    if (rules_on != NULL)
        mrp_htbl_destroy(rules_on , TRUE);
    if (rules_off != NULL)
        mrp_htbl_destroy(rules_off, TRUE);
}


void mrp_debug_reset(void)
{
    debug_enabled = FALSE;
    reset_rules();
}


int mrp_debug_enable(int enabled)
{
    int prev = debug_enabled;

    debug_enabled = !!enabled;
    mrp_log_enable(MRP_LOG_MASK_DEBUG);

    return prev;
}


static int add_rule(const char *func, const char *file, int line, int off)
{
    mrp_htbl_t  *ht;
    char        *rule, *r, buf[PATH_MAX * 2];

    if (rules_on == NULL)
        if (!init_rules())
            return FALSE;

    r = rule = NULL;

    if (!off)
        ht = rules_on;
    else
        ht = rules_off;

    if (func != NULL && file == NULL && line == 0) {
        r    = mrp_htbl_lookup(ht, (void *)func);
        rule = (char *)func;
    }
    else if (func != NULL && file != NULL && line == 0) {
        snprintf(buf, sizeof(buf), "%s@%s", func, file);
        r    = mrp_htbl_lookup(ht, (void *)buf);
        rule = buf;
    }
    else if (func == NULL && file != NULL && line == 0) {
        snprintf(buf, sizeof(buf), "@%s", file);
        r    = mrp_htbl_lookup(ht, (void *)buf);
        rule = buf;
    }
    else if (func == NULL && file != NULL && line > 0) {
        snprintf(buf, sizeof(buf), "%s:%d", file, line);
        r    = mrp_htbl_lookup(ht, (void *)buf);
        rule = buf;
    }

    if (r != NULL)
        return FALSE;

    rule = mrp_strdup(rule);
    if (rule == NULL)
        return FALSE;

    if (mrp_htbl_insert(ht, rule, rule)) {
        mrp_debug_stamp++;

        return TRUE;
    }
    else {
        mrp_free(rule);

        return FALSE;
    }
}


static int del_rule(const char *func, const char *file, int line, int off)
{
    mrp_htbl_t  *ht;
    char        *r, buf[PATH_MAX * 2];

    if (rules_on == NULL)
        if (!init_rules())
            return FALSE;

    r = NULL;

    if (!off)
        ht = rules_on;
    else
        ht = rules_off;

    if (func != NULL && file == NULL && line == 0) {
        r = mrp_htbl_remove(ht, (void *)func, TRUE);
    }
    else if (func != NULL && file != NULL && line == 0) {
        snprintf(buf, sizeof(buf), "%s@%s", func, file);
        r = mrp_htbl_remove(ht, (void *)buf, TRUE);
    }
    else if (func == NULL && file != NULL && line == 0) {
        snprintf(buf, sizeof(buf), "@%s", file);
        r = mrp_htbl_remove(ht, (void *)buf, TRUE);
    }
    else if (func == NULL && file != NULL && line > 0) {
        snprintf(buf, sizeof(buf), "%s:%d", file, line);
        r = mrp_htbl_remove(ht, (void *)buf, TRUE);
    }

    if (r != NULL) {
        mrp_debug_stamp++;

        return TRUE;
    }
    else
        return FALSE;
}


int mrp_debug_set_config(const char *cmd)
{
    char    buf[2 * PATH_MAX + 1], *colon, *at, *eq;
    char   *func, *file, *end;
    size_t  len;
    int     del, off, line;

    if (*cmd == '+' || *cmd == '-')
        del = (*cmd++ == '-');
    else
        del = FALSE;

    eq = strchr(cmd, '=');

    if (eq == NULL) {
        strncpy(buf, cmd, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        off = FALSE;
    }
    else {
        if (!strcmp(eq + 1, "on"))
            off = FALSE;
        else if (!strcmp(eq + 1, "off"))
            off = TRUE;
        else
            return FALSE;

        len = eq - cmd;
        if (len >= sizeof(buf))
            len = sizeof(buf) - 1;

        strncpy(buf, cmd, len);
        buf[len] = '\0';
    }

    colon = strchr(buf, ':');

    if (colon != NULL) {
        if (strchr(buf, '@') != NULL)
            return FALSE;

        *colon = '\0';
        func   = NULL;
        file   = buf;
        line   = strtoul(colon + 1, &end, 10);

        if (end && *end)
            return FALSE;

        mrp_log_info("%s file='%s', line=%d, %s", del ? "del" : "add",
                     file, line, off ? "off" : "on");
    }
    else {
        at = strchr(buf, '@');

        if (at != NULL) {
            *at = '\0';
            func = (at == buf ? NULL : buf);
            file = at + 1;
            line = 0;

            mrp_log_info("%s func='%s', file='%s', %s", del ? "del" : "add",
                         func ? func : "", file, off ? "off" : "on");
        }
        else {
            func = buf;
            file = NULL;
            line = 0;

            mrp_log_info("%s func='%s' %s", del ? "del" : "add",
                         func, off ? "off" : "on");
        }
    }

    if (!del)
        return add_rule(func, file, line, off);
    else
        return del_rule(func, file, line, off);

    return TRUE;
}


typedef struct {
    FILE *fp;
    int   on;
} dump_t;


static int dump_rule_cb(void *key, void *object, void *user_data)
{
    dump_t     *d     = (dump_t *)user_data;
    FILE       *fp    = d->fp;
    const char *state = d->on ? "on" : "off";

    MRP_UNUSED(key);

    fprintf(fp, "    %s %s\n", (char *)object, state);

    return MRP_HTBL_ITER_MORE;
}


int mrp_debug_dump_config(FILE *fp)
{
    dump_t d;

    fprintf(fp, "Debugging is %sabled\n", debug_enabled ? "en" : "dis");

    if (rules_on != NULL) {
        fprintf(fp, "Debugging rules:\n");

        d.fp = fp;
        d.on = TRUE;
        mrp_htbl_foreach(rules_on , dump_rule_cb, &d);
        d.on = FALSE;
        mrp_htbl_foreach(rules_off, dump_rule_cb, &d);
    }
    else
        fprintf(fp, "No debugging rules defined.\n");

    return TRUE;
}

#undef __DUMP_ELF_INFO__

#ifdef __DUMP_ELF_IFDO__
static const char *segment_type(uint32_t type)
{
#define T(type) case type: return #type
    switch (type) {
        T(PT_NULL);
        T(PT_LOAD);
        T(PT_DYNAMIC);
        T(PT_INTERP);
        T(PT_NOTE);
        T(PT_SHLIB);
        T(PT_PHDR);
        T(PT_TLS);
        T(PT_NUM);
        T(PT_LOOS);
        T(PT_GNU_EH_FRAME);
        T(PT_GNU_STACK);
        T(PT_GNU_RELRO);
        T(PT_LOPROC);
        T(PT_HIPROC);
    default:
        return "unknown";
    }
}


static const char *segment_flags(uint32_t flags)
{
    static char buf[4];

    buf[0] = (flags & PF_R) ? 'r' : '-';
    buf[1] = (flags & PF_W) ? 'w' : '-';
    buf[2] = (flags & PF_X) ? 'x' : '-';
    buf[3] = '\0';

    return buf;
}

#endif /* __DUMP_ELF_INFO__ */

typedef struct {
    FILE *fp;
    int   indent;
} list_opt_t;

static int list_cb(struct dl_phdr_info *info, size_t size, void *data)
{
#define P(fmt, args...) fprintf(opt->fp, "%*.*s"fmt,                    \
                                opt->indent, opt->indent, "" , ## args)
#define RELOC(addr) (info->dlpi_addr + addr)

    list_opt_t       *opt = (list_opt_t *)data;
    const ElfW(Phdr) *h;
    int               i;
    const char       *beg, *end, *s, *func;
    char              file[512], *p;
    int               line;

    MRP_UNUSED(size);

#ifdef __DUMP_ELF_INFO__
    P("%s (@%p)\n",
      info->dlpi_name && *info->dlpi_name ? info->dlpi_name : "<none>",
      info->dlpi_addr);
    P("  %d segments\n", info->dlpi_phnum);
#endif

    file[sizeof(file) - 1] = '\0';

    for (i = 0; i < info->dlpi_phnum; i++) {
        h = &info->dlpi_phdr[i];
#if __DUMP_ELF_INFO__
        P("  #%d:\n", i);
        P("       type: 0x%x (%s)\n", h->p_type, segment_type(h->p_type));
        P("     offset: 0x%lx\n", h->p_offset);
        P("      vaddr: 0x%lx (0x%lx)\n", h->p_vaddr, RELOC(h->p_vaddr));
        P("      paddr: 0x%lx (0x%lx)\n", h->p_paddr, RELOC(h->p_paddr));
        P("     filesz: 0x%lx\n", h->p_filesz);
        P("      memsz: 0x%lx\n", h->p_memsz);
        P("      flags: 0x%x (%s)\n", h->p_flags, segment_flags(h->p_flags));
        P("      align: 0x%lx\n", h->p_align);
#endif
        if (h->p_flags & PF_W)
            continue;

        beg = (const char *)RELOC(h->p_vaddr);
        end = (const char *)beg + h->p_memsz;

#define PREFIX     "__DEBUG_SITE_"
#define PREFIX_LEN 13
        for (s = beg; s < end - PREFIX_LEN; s++) {
            if (!strncmp(s, PREFIX, PREFIX_LEN)) {
                s += PREFIX_LEN;
                if (*s != '\0') {
                    strncpy(file, s, sizeof(file) - 1);
                    p = strchr(file, ':');

                    if (p != NULL) {
                        *p = '\0';
                        line = (int)strtoul(p + 1, NULL, 10);
                        func = mrp_debug_site_function(file, line);
                    }
                    else
                        func = NULL;

                    if (func != NULL)
                        P("%s@%s\n", func, s);
                    else
                        P("%s\n", s);
                }
            }
        }
    }

    return 0;
}


void mrp_debug_dump_sites(FILE *fp, int indent)
{
    list_opt_t opt = { .fp = fp, .indent = indent };

    dl_iterate_phdr(list_cb, (void *)&opt);
}


void mrp_debug_msg(const char *site, const char *file, int line,
                   const char *func, const char *format, ...)
{
    va_list ap;

    MRP_UNUSED(site);

    va_start(ap, format);
    mrp_log_msgv(MRP_LOG_DEBUG, file, line, func, format, ap);
    va_end(ap);
}


int mrp_debug_check(const char *func, const char *file, int line)
{
    char  buf[2 * PATH_MAX], *base;
    void *key;

    if (!debug_enabled || rules_on == NULL)
        return FALSE;

    key = (void *)func;
    if (mrp_htbl_lookup(rules_on, key) != NULL)
        goto check_suppress;

    base = strrchr(file, '/');
    if (base != NULL) {
        key = base + 1;
        if (mrp_htbl_lookup(rules_on, key) != NULL)
            goto check_suppress;
    }

    key = buf;

    snprintf(buf, sizeof(buf), "@%s", file);
    if (mrp_htbl_lookup(rules_on, key) != NULL)
        goto check_suppress;

    snprintf(buf, sizeof(buf), "%s@%s", func, file);
    if (mrp_htbl_lookup(rules_on, key) != NULL)
        goto check_suppress;

    snprintf(buf, sizeof(buf), "%s:%d", file, line);
    if (mrp_htbl_lookup(rules_on, key) != NULL)
        goto check_suppress;

    if (mrp_htbl_lookup(rules_on, (void *)WILDCARD) == NULL)
        return FALSE;


 check_suppress:
    if (rules_off == NULL)
        return TRUE;

    key = (void *)func;
    if (mrp_htbl_lookup(rules_off, key) != NULL)
        return FALSE;

    base = strrchr(file, '/');
    if (base != NULL) {
        key = base + 1;
        if (mrp_htbl_lookup(rules_on, key) != NULL)
            return FALSE;
    }

    key = buf;

    snprintf(buf, sizeof(buf), "@%s", file);
    if (mrp_htbl_lookup(rules_off, key) != NULL)
        return FALSE;

    snprintf(buf, sizeof(buf), "%s@%s", func, file);
    if (mrp_htbl_lookup(rules_off, key) != NULL)
        return FALSE;

    snprintf(buf, sizeof(buf), "%s:%d", file, line);
    if (mrp_htbl_lookup(rules_off, key) != NULL)
        return FALSE;

    return TRUE;
}


int mrp_debug_register_file(mrp_debug_file_t *df)
{
    mrp_list_append(&debug_files, &df->hook);

    return TRUE;
}


int mrp_debug_unregister_file(mrp_debug_file_t *df)
{
    mrp_list_delete(&df->hook);

    if (files != NULL)
        mrp_htbl_remove(files, (void *)df->file, FALSE);

    return TRUE;
}


const char *mrp_debug_site_function(const char *file, int line)
{
    mrp_debug_info_t *info;
    const char       *func;

    if (files == NULL)
        populate_file_table();

    func = NULL;

    if (files != NULL) {
        info = mrp_htbl_lookup(files, (void *)file);

        if (info != NULL) {
            while (info->func != NULL) {
                if (info->line < line) {
                    func = info->func;
                    info++;
                }
                else
                    break;
            }
        }
    }

    return func;
}


static void populate_file_table(void)
{
    mrp_htbl_config_t  hcfg;
    mrp_debug_file_t  *df;
    mrp_list_hook_t   *p, *n;

    if (files == NULL) {
        mrp_clear(&hcfg);
        hcfg.comp = mrp_string_comp;
        hcfg.hash = mrp_string_hash;

        files = mrp_htbl_create(&hcfg);
    }

    if (files != NULL) {
        mrp_list_foreach(&debug_files, p, n) {
            df = mrp_list_entry(p, typeof(*df), hook);

            mrp_htbl_insert(files, (void *)df->file, df->info);
        }
    }
}


static void flush_file_table(void)
{
    mrp_debug_file_t  *df;
    mrp_list_hook_t   *p, *n;

    if (files != NULL) {
        mrp_htbl_reset(files, FALSE);
        files = NULL;

        mrp_list_foreach(&debug_files, p, n) {
            df = mrp_list_entry(p, typeof(*df), hook);

            mrp_htbl_insert(files, (void *)df->file, df->info);
        }
    }
}


