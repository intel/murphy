#ifndef __MQL_TRIGGER_H__
#define __MQL_TRIGGER_H__

typedef struct mql_result_s  mql_result_t;

typedef void (*mql_trigger_cb_t)(mql_result_t *, void *);

int mql_register_callback(const char *, mql_result_type_t,
                          mql_trigger_cb_t, void *);
int mql_unregister_callback(const char *);


#endif /* __MQL_TRIGGER_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
