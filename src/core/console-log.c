
static void log_level(mrp_console_t *c, void *user_data,
                      int argc, char **argv)
{
    mrp_log_mask_t mask;
    char           buf[256];

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    if (argc == 2)
        mask = mrp_log_get_mask();
    else {
        mask = mrp_log_parse_levels(argv[2]);
        mrp_log_set_mask(mask);
    }

    printf("current logging mask: %s\n",
           mrp_log_dump_mask(mask, buf, sizeof(buf)));
}


static void log_target(mrp_console_t *c, void *user_data,
                       int argc, char **argv)
{
    const char *target;
    const char *targets[32];
    int         i, n;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    if (argc == 2) {
        target = mrp_log_get_target();
        n      = mrp_log_get_targets(targets, MRP_ARRAY_SIZE(targets));

        printf("available log targets:\n");
        for (i = 0; i < n; i++)
            printf("    %s%s\n", targets[i],
                   !strcmp(targets[i], target) ? " (active)" : "");
    }
    else if (argc == 3) {
        target = argv[2];

        if (!mrp_log_set_target(target))
            printf("failed to change logging target to %s", target);
        else {
            printf("changed log target to %s\n", target);
            mrp_log_info("changed log target to %s", target);
        }
    }
    else {
        printf("%s/%s invoked with wrong number of arguments\n",
               argv[0], argv[1]);
    }
}




#define LOG_GROUP_DESCRIPTION                                               \
    "Log commands provide means to configure the active logging settings\n" \
    "of Murphy. Commands are provided for changing the logging level,\n"    \
    "listing log targets, and settting the active target.\n"

#define LEVEL_SYNTAX      "[[info[,warning[,error]]]]"
#define LEVEL_SUMMARY     "change or show the active logging level"
#define LEVEL_DESCRIPTION \
    "Changes the logging level to the given one. Without arguments it\n" \
    "prints out the current logging level.\n"

#define TARGET_SYNTAX      "[stdout|stderr|syslog|<other targets>]"
#define TARGET_SUMMARY     "change or show the active logging target"
#define TARGET_DESCRIPTION \
    "Changes the active logging target to the given one. Without arguments\n" \
    "it lists the available targets and the currently active one."

MRP_CORE_CONSOLE_GROUP(log_group, "log", LOG_GROUP_DESCRIPTION, NULL, {
        MRP_TOKENIZED_CMD("level" , log_level , FALSE,
                          LEVEL_SYNTAX , LEVEL_SUMMARY , LEVEL_DESCRIPTION),
        MRP_TOKENIZED_CMD("target", log_target, FALSE,
                          TARGET_SYNTAX, TARGET_SUMMARY, TARGET_DESCRIPTION)
});
