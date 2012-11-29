#ifndef foomurphyresourceapifoo
#define foomurphyresourceapifoo

#include <murphy/common.h>

typedef struct murphy_resource_context_private_s murphy_resource_context_private_t;
typedef struct murphy_resource_private_s         murphy_resource_private_t;
typedef struct murphy_resource_set_private_s     murphy_resource_set_private_t;

typedef enum murphy_connection_state_ {
    murphy_connected,
    murphy_disconnected,
} murphy_connection_state;

typedef enum murphy_resource_state_ {
    murphy_resource_lost,
    murphy_resource_pending,
    murphy_resource_acquired,
    murphy_resource_available,
} murphy_resource_state;

typedef enum murphy_resource_error_ {
    murphy_resource_error_none,
    murphy_resource_error_connection_lost,
    murphy_resource_error_internal,
    murphy_resource_error_malformed,
} murphy_resource_error;

typedef struct murphy_resource_context_ {
    murphy_connection_state state;
    murphy_resource_context_private_t *priv;
} murphy_resource_context;

typedef enum murphy_attribute_type_ {
    murphy_int32   = 'i',
    murphy_uint32  = 'u',
    murphy_double  = 'f',
    murphy_string  = 's',
    murphy_invalid = '\0'
} murphy_attribute_type;

typedef struct murphy_resource_attribute_ {
    const char            *name;
    murphy_attribute_type  type;
    union {
        const char *string;
        int32_t     integer;
        uint32_t    unsignd;
        double      floating;
    };
} murphy_resource_attribute;

typedef struct murphy_resource_ {
    const char                *name;
    murphy_resource_state      state;
    murphy_resource_private_t *priv;
} murphy_resource;

typedef struct murphy_resource_set_ {
    const char                    *application_class;
    murphy_resource_state          state;
    murphy_resource_set_private_t *priv;
} murphy_resource_set;

typedef struct murphy_string_array_ {
    int num_strings;
    const char **strings;
} murphy_string_array;

/**
 * Prototype for murphy state callback. You have to be in
 * connected state before you can do any operation with
 * resources.
 *
 * @param cx murphy connection context.
 * @param err error message.
 * @param userdata data you gave when starting to connect.
 */
typedef void (*murphy_state_callback) (murphy_resource_context *cx,
                       murphy_resource_error err,
                       void *userdata);

/**
 * Prototype for resource update callback. All changes related to
 * your acquired resource set is handled through this function.
 * It is up to you to decide what change in the set is important
 * for you. This is an update to the set created by you and you
 * can find the differences by comparison.
 *
 * @param cx murphy connection context.
 * @param set updated resource set for you to handle.
 * @param userdata data you gave when starting to acquire resources.
 */
typedef void (*murphy_resource_callback) (murphy_resource_context *cx,
                      const murphy_resource_set *set,
                      void *userdata);

/**
 * Connect to murphy. You have to wait for the callback
 * to check that state is connected.
 *
 * @param ml pointer to murphy mainloop.
 * @param cb connection state callback from Murphy resource engine.
 * @param userdata pointer to possible data you want to access in
 * state callback.
 *
 * @return pointer to the newly created resource context.
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
 * library when creating the connection so it will be synchronous.
 */
const murphy_string_array * murphy_application_class_list(murphy_resource_context *cx);

/**
 * List all possible resources that you can try to acquire. This info
 * is cached to the client library when creating connection so it
 * will be synchronous. This is a "master" resource set you can't
 * modify or use as your own resource set. It is only meant for
 * introspecting the possible resources.
 */
const murphy_resource_set * murphy_resource_set_list(murphy_resource_context *cx);

/**
 * Create new empty resource set. This is a resource set allocated
 * for you so you have to remember to release it.
 *
 * @param cx murphy connection context.
 * @param app_class application class for the resource set.
 * @param cb resource update callback.
 * @param userdata data you want to access in resource callback.
 *
 * @return pointer to a new empty resource set.
 */
murphy_resource_set * murphy_resource_set_create(murphy_resource_context *cx,
        const char *app_class,
        murphy_resource_callback cb,
        void *userdata);

/**
 * Delete resource set created with murphy_resource_set_create
 * or murphy_resource_set_copy.
 *
 * @param set pointer to existing resource set created by the user.
 */
void murphy_resource_set_delete(murphy_resource_set *set);

/**
 * Make a copy of the resource set. This is a helper function to
 * be used for example when you receive updated resource set in
 * resource callback.
 *
 * @param original resource set to be copied.
 *
 * @return pointer to a copy of the resource set.
 */
murphy_resource_set *murphy_resource_set_copy(const murphy_resource_set *orig);

/**
 * You might have assigned the same update callback for
 * several resource sets and you have to identify the
 * updated set. You can compare your locale copy to
 * find out if the update concerns that particular set.
 *
 * @param a set to be used in comparison
 * @param b set to be used in comparison
 *
 * @return true when matching, false otherwise.
 */
bool murphy_resource_set_equals(const murphy_resource_set *a,
                const murphy_resource_set *b);

/**
 * Acquire resources. Errors in the return value will
 * indicate only connection problems or malformed
 * resource structs. If you will be granted the resources
 * you asked for you will get an update for your resource
 * set in the resource callback.
 *
 * @param cx connnection to Murphy resource engine.
 * @param set resource set you want to acquire.
 *
 * @return murphy error code.
 */
int murphy_resource_set_acquire(murphy_resource_context *cx,
                murphy_resource_set *set);

/**
 * Release the acquired resource set. Resource callbacks
 * for this set will obviously stop.
 *
 * @param cx connnection to Murphy resource engine.
 * @param set resource set you want to release.
 *
 * @return murphy error code.
 */
int murphy_resource_set_release(murphy_resource_context *cx,
                murphy_resource_set *set);

/**
 * Create new resource by name and init all other fields.
 * Created resource will be automatically added to
 * the resource set provided as argument.
 *
 * @param cx connnection to Murphy resource engine.
 * @param set resource the resource will be added to.
 * @param name name of the resource you want to create.
 * @param mandatory is the resource mandatory or not
 * @param shared can the resource be shared or not
 *
 * @return pointer to new resource if succesful null otherwise.
 */
murphy_resource *murphy_resource_create(murphy_resource_context *cx,
                    murphy_resource_set *set,
                    const char *name,
                    bool mandatory,
                    bool shared);

/**
 * Get the names of all resources in this resource set.
 *
 * @param cx murphy context.
 * @param rs resource set where the resource are.
 * @param names pointer where the name array with content will
 * be allocated.
 *
 * @return murphy error code
 */
murphy_string_array * murphy_resource_list_names(murphy_resource_context *cx,
                const murphy_resource_set *rs);

/**
 * Delete resource by name from resource set.
 *
 * @param cx connnection to Murphy resource engine.
 * @param rs resource set where you want to get the resource.
 * @param name name of the resource you want to get.
 * @param pointer to resource pointer to be assigned.
 *
 * @return 0 if resource found.
 */
murphy_resource * murphy_resource_get_by_name(murphy_resource_context *cx,
                    const murphy_resource_set *rs,
                    const char *name);

/**
 * Delete a resource from a resource set.
 *
 * @param set resource the resource will deleted from.
 * @param res resource to be deleted.
 *
 */
void murphy_resource_delete(murphy_resource_set *set,
                murphy_resource *res);

/**
 * Delete resource by name from resource set.
 *
 * @param rs resource set where you want to remove the resource.
 * @param name name of the resource you want to remove.
 *
 * @return true if resource found and removed.
 */
bool murphy_resource_delete_by_name(murphy_resource_set *rs,
                    const char *name);

/**
 * Get the names of all attributes in this resource.
 *
 * @param cx murphy context.
 * @param res resource where the attributes are taken.
 * @param names pointer where the name array with content will
 * be allocated.
 *
 * @return murphy error code
 */
murphy_string_array * murphy_attribute_list_names(murphy_resource_context *cx,
                const murphy_resource *res);

/**
 * Get the particular resource attribute by name from the resource.
 *
 * @param cx murphy context.
 * @param res resource where the attributes are taken.
 * @param name of the attribute that is fetched.
 * @param attribute pointer that will be allocated with the attribute.
 *
 * @return murphy error code.
 */
murphy_resource_attribute * murphy_attribute_get_by_name(murphy_resource_context *cx,
                 murphy_resource *res,
                 const char *name);

/**
 * Set new attribute value to resource.
 *
 * @param cx murphy context.
 * @param res resource where the attribute is set.
 * @param attribute value to be set as new attribute replacing the old.
 *
 * @return murphy error code.
 */
int murphy_attribute_set(murphy_resource_context *cx,
             murphy_resource *res,
             const murphy_resource_attribute *attr);

#endif /* foomurphyresourceapifoo */
