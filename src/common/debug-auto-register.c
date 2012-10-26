static void __attribute__((constructor)) register_debug_data(void)
{
    mrp_debug_file_t *df;
    int               i;

    for (i = 0; (df = debug_files[i]) != NULL; i++)
        mrp_debug_register_file(df);
}

static void __attribute__((destructor)) unregister_debug_data(void)
{
    mrp_debug_file_t *df;
    int               i;

    for (i = 0; (df = debug_files[i]) != NULL; i++)
        mrp_debug_unregister_file(df);
}

