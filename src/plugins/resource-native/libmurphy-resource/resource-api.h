#ifndef foomurphyresourceapifoo
#define foomurphyresourceapifoo

#include <murphy/common.h>

#define MAX_LEN 64

typedef struct murphy_resource_context_private_s murphy_resource_context_private_t;
typedef struct murphy_resource_private_s         murphy_resource_private_t;
typedef struct murphy_resource_set_private_s     murphy_resource_set_private_t;


enum murphy_connection_state {
    murphy_connected,
    murphy_disconnected,
};

enum murphy_resource_state {
    murphy_resource_lost,
    murphy_resource_pending,
    murphy_resource_acquired,
    murphy_resource_available,
};

enum murphy_resource_error {
    murphy_resource_error_none,
    murphy_resource_error_connection_lost,
    murphy_resource_error_internal,
    murphy_resource_error_malformed,
};

typedef struct murphy_resource_context_ {
    enum murphy_connection_state state;
    murphy_resource_context_private_t *priv;
} murphy_resource_context;

typedef struct murphy_resource_ {
    char name[MAX_LEN];
    enum murphy_resource_state state;
    bool mandatory;
    bool shared;
    char properties[MAX_LEN];
    murphy_resource_private_t *priv;
} murphy_resource;

typedef struct murphy_resource_set_ {
    uint32_t id;

    char application_class[MAX_LEN];
    enum murphy_resource_state state;

    int num_resources;
    murphy_resource *resources[MAX_LEN];

    murphy_resource_set_private_t *priv;
} murphy_resource_set;

enum murphy_attribute_type {
    murphy_int32,
    murphy_uint32,
    murphy_double,
    murphy_string,
    murphy_invalid
};


/**
 * Prototype for murphy state callback. You have to be in
 * connected state before you can do any operation with
 * resources.
 *
 * @param err error message.
 * @param userdata data you gave when starting to connect.
 */
typedef void (*murphy_state_callback) (murphy_resource_context *cx, enum murphy_resource_error err, void *userdata);

/**
 * Prototype for resource update callback. All changes related to
 * your acquired resource set is handled through this function.
 * It is up to you to decide what change in the set is important
 * for you. This is an update to the set created by you and you
 * can find the differences by comparison.
 *
 * @param set updated resource set for you to handle.
 * @param userdata data you gave when starting to acquire resources.
 */
typedef void (*murphy_resource_callback) (murphy_resource_context *cx, murphy_resource_set *set, void *userdata);

/**
 * Connect to murphy. You have to wait for the callback
 * to check that state is connected.
 *
 * @param cb connection state callback from Murphy resource engine.
 * @param pointer to possible data you want to access in state callback.
 *
 * @return pointer to the new resource context.
 */
murphy_resource_context *murphy_create(mrp_mainloop_t *ml,
               murphy_state_callback cb,
               void *userdata);

/**
 * Disconnect from murphy.
 *
 * @param cx Murphy connection context to destroy.
 */
void murphy_destroy(murphy_resource_context *cx);

/**
 * List possible application classes that you can assign yourself
 * when asking for resources. This info is cached to the client
 * library when creating connection so it will be synchronous.
 */
int murphy_list_application_classes(murphy_resource_context *cx,
            const char ***app_classes, int *num_classes);

/**
 * List possible resources that you can try to acquire. This info
 * is cached to the client library when creating connection so it
 * will be synchronous.
 */
int murphy_list_resources(murphy_resource_context *cx, murphy_resource_set **set);

/**
 * Identify your application class, acquire resources you want and
 * assign callback to resource change message.
 *
 * @param cx connnection to Murphy resource engine.
 * @param app name to identify your application class.
 * @param set resource set you want to acquire.
 * @param cb callback for your resource set update.
 *
 */
int murphy_acquire_resources(murphy_resource_context *cx,
                murphy_resource_set *set);


int murphy_release_resources(murphy_resource_context *cx,
                murphy_resource_set *set);


/**
 * Create new empty resource set.
 *
 * @param app_class application class for the resource set.
 *
 * @return pointer to a new empty resource set.
 */
murphy_resource_set *murphy_create_resource_set(const char *app_class);


murphy_resource_set *murphy_copy_resource_set(murphy_resource_set *original);


/* The resource sets are the "same" from the server point of view if the
 * id is the same. */
bool murphy_same_resource_sets(murphy_resource_set *a, murphy_resource_set *b);

void murphy_set_resource_set_callback(murphy_resource_set *set,
                murphy_resource_callback cb,
                void *userdata);

/**
 * Delete resource set.
 *
 * @param set pointer to existing resource set.
 */
void murphy_delete_resource_set(murphy_resource_set *set);

/**
 * Create new resource by name and init all other fields.
 *
 * @param name name of the resource you want to create.
 * @param mandatory is the resource mandatory or not
 * @param shared can the resource be shared or not
 *
 * @return pointer to new resource if succesful null otherwise.
 */
murphy_resource *murphy_resource_create(murphy_resource_context *cx, const char *name,
            bool mandatory, bool shared); /* , murphy_resource_set */

/**
 * Delete a resource.
 *
 * @param res resource to be deleted.
 *
 */
void murphy_resource_delete(murphy_resource *res);

/**
 * Add resource to existing resource set.
 * Returns the index of this resource in the
 * resource set or -1 in the case of error.
 *
 * @param rs resource set where you want to add a resource.
 * @param res pointer to resource you want to add to the set.
 *
 * @return index of the new resource in the set or -1 in
 * the case of error.
 */
int murphy_add_resource(murphy_resource_set *rs, murphy_resource *res);

/**
 * Remove resource by name from resource set.
 *
 * @param rs resource set where you want to remove the resource.
 * @param name name of the resource you want to remove.
 *
 * @return true if resource found and removed.
 */
bool murphy_remove_resource_by_name(murphy_resource_set *rs, const char *name);



char **get_attribute_names(murphy_resource_context *cx, murphy_resource
                *res);

enum murphy_attribute_type get_attribute_type(murphy_resource_context *cx, murphy_resource
                *res, const char *attribute_name);

/**
 * @return pointer to the actual value.
 */
const void *murphy_get_attribute(murphy_resource_context *cx, murphy_resource
                *res, const char *attribute_name);

/**
 * @param value is a pointer to the actual value.
 */
bool murphy_set_attribute(murphy_resource_context *cx, murphy_resource
                *res, const char *attribute_name, void *value);

#endif /* foomurphyresourceapifoo */
