#include <stdio.h>
#include <string.h>

#include <murphy-db/mqi-types.h>

const char *mqi_data_type_str(mqi_data_type_t type)
{
    switch (type) {
    case mqi_integer:  return "integer";
    case mqi_unsignd:  return "unsigned";
    case mqi_varchar:  return "varchar";
    case mqi_blob:     return "blob";
    default:           return "unknown";
    }
}

int mqi_data_compare_integer(int datalen, void *data1, void *data2)
{
    int32_t integer1;
    int32_t integer2;

    if (datalen != sizeof(int32_t) || !data1 || !data2)
        return 0;

    integer1 = *(int32_t *)data1;
    integer2 = *(int32_t *)data2;


    return (integer1 - integer2);
}

int mqi_data_compare_unsignd(int datalen, void *data1, void *data2)
{
    uint32_t unsigned1;
    uint32_t unsigned2;

    if (datalen != sizeof(uint32_t) || !data1 || !data2)
        return 0;

    unsigned1 = *(uint32_t *)data1;
    unsigned2 = *(uint32_t *)data2;

    if (unsigned1 < unsigned2)
        return -1;

    if (unsigned1 > unsigned2)
        return 1;

    return 0;
}

int mqi_data_compare_string(int datalen, void *data1, void *data2)
{
    const char *varchar1 = (const char *)data1;
    const char *varchar2 = (const char *)data2;

    (void)datalen;

    if (!varchar1 || !varchar1[0] || !varchar2 || !varchar2[0])
        return 0;

    return strcmp(varchar1, varchar2);
}

int mqi_data_compare_pointer(int datalen, void *data1, void *data2)
{
    (void)datalen;

    return (data1 - data2);
}

int mqi_data_compare_varchar(int datalen, void *data1, void *data2)
{
    return mqi_data_compare_string(datalen, data1, data2);
}

int mqi_data_compare_blob(int datalen, void *data1, void *data2)
{
    if (!datalen || !data1 || !data2)
        return 0;

    return memcmp(data1, data2, datalen);
}

int mqi_data_print_integer(void *data, char *buf, int len)
{
    int32_t integer = *(int32_t *)data;

    return snprintf(buf, len, "%d", integer);
}

int mqi_data_print_unsignd(void *data, char *buf, int len)
{
    uint32_t unsignd = *(uint32_t *)data;

    return snprintf(buf, len, "%u", unsignd);
}

int mqi_data_print_string(void *data, char *buf, int len)
{
    const char *varchar = (const char *)data;

    return snprintf(buf, len, "%s", varchar);
}

int mqi_data_print_pointer(void *data, char *buf, int len)
{
    return snprintf(buf, len, "%p", data);
}

int mqi_data_print_varchar(void *data, char *buf, int len)
{
    const char *varchar = (const char *)data;

    return snprintf(buf, len, "%s", varchar);
}

int mqi_data_print_blob(void *data, char *buf, int len)
{
    return 0;
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
