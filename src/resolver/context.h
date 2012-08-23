#ifndef __MRP_RESOLVER_CONTEXT_H__
#define __MRP_RESOLVER_CONTEXT_H__

int init_context_table(mrp_resolver_t *r);
void cleanup_context_table(mrp_resolver_t *r);
int declare_context_variable(mrp_resolver_t *r, const char *name,
                             mrp_script_type_t type);

int push_context_frame(mrp_resolver_t *r);
int pop_context_frame(mrp_resolver_t *r);
int get_context_id(mrp_resolver_t *r, const char *name);
int get_context_value(mrp_resolver_t *r, int id, mrp_script_value_t *value);
int set_context_values(mrp_resolver_t *r, int *ids, mrp_script_value_t *values,
                       int nid);
int set_context_value(mrp_resolver_t *r, int id, mrp_script_value_t *value);

#endif /* __MRP_RESOLVER_CONTEXT_H__ */
