/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#ifndef __MURPHY_RESOURCE_API_H__
#define __MURPHY_RESOURCE_API_H__

#include <stdarg.h>

/*
 * Enable the json-c/JSON-Glib symbol clash hackaround in transport.h.
 * This is currently the only known place which triggers the symbol
 * clash. It happens when compiling ico-uxf-homescreen which includes
 * this indirectly and also uses JSON-Glib internally for manipulating
 * JSON objects...
 */

#define __JSON_GLIB_DANGER__

#include <murphy/common.h>

MRP_CDECL_BEGIN

typedef struct mrp_res_context_private_s mrp_res_context_private_t;
typedef struct mrp_res_resource_private_s mrp_res_resource_private_t;
typedef struct mrp_res_resource_set_private_s mrp_res_resource_set_private_t;

typedef enum {
    MRP_RES_CONNECTED,
    MRP_RES_DISCONNECTED,
} mrp_res_connection_state_t;

typedef enum {
    MRP_RES_RESOURCE_LOST,
    MRP_RES_RESOURCE_PENDING,
    MRP_RES_RESOURCE_ACQUIRED,
    MRP_RES_RESOURCE_AVAILABLE,
} mrp_res_resource_state_t;

typedef enum {
    MRP_RES_ERROR_NONE,
    MRP_RES_ERROR_CONNECTION_LOST,
    MRP_RES_ERROR_INTERNAL,
    MRP_RES_ERROR_MALFORMED,
} mrp_res_error_t;

typedef struct {
    mrp_res_connection_state_t state;
    const char *zone;
    mrp_res_context_private_t *priv;
} mrp_res_context_t;

typedef enum {
    mrp_int32   = 'i',
    mrp_uint32  = 'u',
    mrp_double  = 'f',
    mrp_string  = 's',
    mrp_invalid = '\0'
} mrp_res_attribute_type_t;

typedef struct {
    const char *name;
    mrp_res_attribute_type_t type;
    union {
        const char *string;
        int32_t integer;
        uint32_t unsignd;
        double floating;
    };
} mrp_res_attribute_t;

typedef struct {
    const char *name;
    mrp_res_resource_state_t state;
    mrp_res_resource_private_t *priv;
} mrp_res_resource_t;

typedef struct {
    const char *application_class;
    mrp_res_resource_state_t state;
    mrp_res_resource_set_private_t *priv;
} mrp_res_resource_set_t;

typedef struct {
    int num_strings;
    const char **strings;
} mrp_res_string_array_t;

/**
 * Prototype for murphy state callback. You have to be in
 * connected state before you can do any operation with
 * resources.
 *
 * @param cx murphy connection context.
 * @param err error message.
 * @param userdata data you gave when starting to connect.
 */
typedef void (*mrp_res_state_callback_t) (mrp_res_context_t *cx,
        mrp_res_error_t err, void *userdata);

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
typedef void (*mrp_res_resource_callback_t) (mrp_res_context_t *cx,
        const mrp_res_resource_set_t *rs, void *userdata);

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
mrp_res_context_t *mrp_res_create(mrp_mainloop_t *ml,
        mrp_res_state_callback_t cb, void *userdata);

/**
 * Disconnect from murphy.
 *
 * @param cx Murphy connection context to destroy.
 */
void mrp_res_destroy(mrp_res_context_t *cx);

/**
 * List possible application classes that you can assign yourself
 * when asking for resources. This info is cached to the client
 * library when creating the connection so it will be synchronous.
 */
const mrp_res_string_array_t * mrp_res_list_application_classes(
        mrp_res_context_t *cx);

/**
 * List all possible resources that you can try to acquire. This info
 * is cached to the client library when creating connection so it
 * will be synchronous. This is a "master" resource set you can't
 * modify or use as your own resource set. It is only meant for
 * introspecting the possible resources.
 */
const mrp_res_resource_set_t * mrp_res_list_resources(mrp_res_context_t *cx);

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
mrp_res_resource_set_t * mrp_res_create_resource_set(mrp_res_context_t *cx,
        const char *app_class, mrp_res_resource_callback_t cb, void *userdata);

/**
 * Set automatic release mode to the resource set. This means that if an
 * application loses the resource set, it doesn't automatically get it back
 * when the resource becomes available again. By default the automatic
 * release mode is off.
 *
 * @param status automatic release status: TRUE means on, FALSE means off
 * @param rs resource set that is being updated.
 *
 * @return true if successful, false otherwise.
 */
bool mrp_res_set_autorelease(bool status,
        mrp_res_resource_set_t *rs);

/**
 * Delete resource set created with mrp_res_create_resource_set
 * or mrp_res_copy_resource_set.
 *
 * @param set pointer to existing resource set created by the user.
 */
void mrp_res_delete_resource_set(mrp_res_resource_set_t *rs);

/**
 * Make a copy of the resource set. This is a helper function to
 * be used for example when you receive updated resource set in
 * resource callback.
 *
 * @param original resource set to be copied.
 *
 * @return pointer to a copy of the resource set.
 */
mrp_res_resource_set_t *mrp_res_copy_resource_set(const mrp_res_resource_set_t *orig);

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
bool mrp_res_equal_resource_set(const mrp_res_resource_set_t *a,
        const mrp_res_resource_set_t *b);

/**
 * Acquisition and release:
 *
 * These two functions serve two purposes. First,
 * they start the attempt of acquisition or release of
 * a set of resources. Second, both of them - if
 * successful - start the delivery of Resource
 * callbacks to the calling application regarding
 * the affected resource set.
 *
 * What the second point means is that, in case an
 * application wants to know the state of resources,
 * but not actually acquire them, it is valid to
 * "release" a set containing these resources
 * without acquiring them first.
 */

/**
 * Acquire resources. Errors in the return value will
 * indicate only connection problems or malformed
 * resource structs. If you will be granted the resources
 * you asked for you will get an update for your resource
 * set in the resource callback.
 *
 * @param rs resource set you want to acquire.
 *
 * @return murphy error code.
 */
int mrp_res_acquire_resource_set(const mrp_res_resource_set_t *rs);

/**
 * Release a resource set. Releasing a set of resources
 * will not stop delivery of Resource callbacks for that
 * set, updates for its status will still be delivered.
 *
 * This function can be called even with not yet acquired
 * sets in order to start delivery of Resource callbacks
 * for them, which can be useful for applications wishing
 * to survey the state of specific set of resources without
 * actually affecting it.
 *
 * @param rs resource set you want to release.
 *
 * @return murphy error code.
 */
int mrp_res_release_resource_set(mrp_res_resource_set_t *rs);


/**
 * Get a resource set unique server-side id. The id information is
 * normally available only after mrp_res_acquire_resource_set or
 * mrp_res_release_resource_set function callback has been called.
 *
 * The id is the resource set internal id, available on the resource
 * manager side. The client can use this information to associate other
 * properties with the resource set. The resource manager can then use
 * this extra information to process system events.
 *
 * An example would be to set an audio stream property to contain the
 * resource set id. The resource manager can use the data associated
 * with audio streams to find out which streams belong to which resource
 * set in the audio domain controller.
 *
 * @param rs resource set whose id is queried.
 *
 * @return resource set id.
 **/
int mrp_res_get_resource_set_id(mrp_res_resource_set_t *rs);


/**
 * Create new resource by name and init all other fields.
 * Created resource will be automatically added to
 * the resource set provided as argument.
 *
 * @param set resource the resource will be added to.
 * @param name name of the resource you want to create.
 * @param mandatory is the resource mandatory or not
 * @param shared can the resource be shared or not
 *
 * @return pointer to new resource if succesful null otherwise.
 */
mrp_res_resource_t *mrp_res_create_resource(mrp_res_resource_set_t *rs,
        const char *name, bool mandatory,
        bool shared);

/**
 * Get the names of all resources in this resource set.
 *
 * @param rs resource set where the resource are.
 *
 * @return string array that needs to be freed with mrp_res_free_string_array
 */
mrp_res_string_array_t * mrp_res_list_resource_names(
        const mrp_res_resource_set_t *rs);

/**
 * Delete resource by name from resource set.
 *
 * @param rs resource set where you want to get the resource.
 * @param name name of the resource you want to get.
 * @param pointer to resource pointer to be assigned.
 *
 * @return 0 if resource found.
 */
mrp_res_resource_t * mrp_res_get_resource_by_name(
        const mrp_res_resource_set_t *rs, const char *name);

/**
 * Delete a resource from a resource set.
 *
 * @param res resource to be deleted.
 *
 */
void mrp_res_delete_resource(mrp_res_resource_t *res);

/**
 * Delete resource by name from resource set.
 *
 * @param rs resource set where you want to remove the resource.
 * @param name name of the resource you want to remove.
 *
 * @return true if resource found and removed.
 */
bool mrp_res_delete_resource_by_name(mrp_res_resource_set_t *rs,
        const char *name);

/**
 * Get the names of all attributes in this resource.
 *
 * @param res resource where the attributes are taken.
 *
 * @return string array that needs to be freed with mrp_res_free_string_array
 */
mrp_res_string_array_t * mrp_res_list_attribute_names(
        const mrp_res_resource_t *res);

/**
 * Get the particular resource attribute by name from the resource.
 *
 * @param res resource where the attributes are taken.
 * @param name of the attribute that is fetched.
 *
 * @return attribute pointer to the fetched attribute.
 */
mrp_res_attribute_t * mrp_res_get_attribute_by_name(mrp_res_resource_t *res,
        const char *name);

/**
 * Set new string attribute value to resource.
 *
 * @param attr attríbute pointer returned by mrp_res_get_attribute_by_name.
 * @value value to be set, copied by the library.
 *
 * @return murphy error code.
 */
int mrp_res_set_attribute_string(mrp_res_attribute_t *attr,
        const char *value);


/**
 * Set new unsigned integer attribute value to resource.
 *
 * @param attr attríbute pointer returned by mrp_res_get_attribute_by_name.
 * @value value to be set.
 *
 * @return murphy error code.
 */
int mrp_res_set_attribute_uint(mrp_res_attribute_t *attr,
        uint32_t value);


/**
 * Set new integer attribute value to resource.
 *
 * @param attr attríbute pointer returned by mrp_res_get_attribute_by_name.
 * @value value to be set.
 *
 * @return murphy error code.
 */
int mrp_res_set_attribute_int(mrp_res_attribute_t *attr,
        int32_t value);


/**
 * Set new unsigned integer attribute value to resource.
 *
 * @param attr attríbute pointer returned by mrp_res_get_attribute_by_name.
 * @value value to be set.
 *
 * @return murphy error code.
 */
int mrp_res_set_attribute_double(mrp_res_attribute_t *attr,
        double value);


/**
 * Free a string array.
 *
 * @param arr string array to be freed.
 */
void mrp_res_free_string_array(mrp_res_string_array_t *arr);


/**
 * Prototype for an external logger.
 *
 * @param level log level.
 * @param file source file (__FILE__) he log message originated from.
 * @param line source line (__LINE__) the log message originated from.
 * @param func function (__func__) the log message originated from.
 *
 * @return none.
 */
typedef void (*mrp_res_logger_t) (mrp_log_level_t level, const char *file,
                                  int line, const char *func,
                                  const char *format, va_list args);

/**
 * Set an external logger for the resource library. All log messages
 * produced by the library will be handed to this function. If you
 * want to suppress all logs by the library, set the logger to NULL.
 *
 * @param logger the logger function to use.
 *
 * @return pointer to the previously active logger function.
 */

mrp_res_logger_t mrp_res_set_logger(mrp_res_logger_t logger);

MRP_CDECL_END

#endif /* __MURPHY_RESOURCE_API_H__ */
