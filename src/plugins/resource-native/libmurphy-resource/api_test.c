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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <murphy/plugins/resource-native/libmurphy-resource/resource-api.h>

typedef struct my_app_data {
    mrp_res_context_t *cx;
    mrp_res_resource_set_t *rs;
} my_app_data;


static bool accept_input;

/* state callback for murphy connection */
static void state_callback(mrp_res_context_t *cx,
               mrp_res_error_t,
               void *ud);

/* callback for resource set update */
static void resource_callback(mrp_res_context_t *cx,
                  const mrp_res_resource_set_t *rs,
                  void *userdata);

void create_resources(my_app_data *app_data)
{
    mrp_res_resource_t *resource = NULL;
    mrp_res_attribute_t *attr;

    /* if we already have a decent set, just re-acquire it */
    if (app_data->rs) {
        mrp_res_acquire_resource_set(app_data->rs);
        return;
    }

    /* otherwise create resource set and resources */
    app_data->rs = mrp_res_create_resource_set(app_data->cx,
                          "player",
                          resource_callback,
                          (void*)app_data);

    if (app_data->rs == NULL) {
        printf("Couldn't create resource set\n");
        return;
    }

    if (!mrp_res_set_autorelease(TRUE, app_data->rs)) {
        printf("Could not set autorelease flag!\n");
        return;
    }

    resource = mrp_res_create_resource(app_data->rs,
                      "audio_playback",
                      true,
                      false);

    if (resource == NULL) {
        printf("Couldn't create audio resource\n");
        mrp_res_delete_resource_set(app_data->rs);
        return;
    }

    /* set a resource attribute */

    attr = mrp_res_get_attribute_by_name(resource, "role");

    if (attr) {
        mrp_res_set_attribute_string(attr, "call");
    }

    resource = mrp_res_create_resource(app_data->rs,
                      "video_playback",
                      true,
                      false);

    if (resource == NULL) {
        printf("Couldn't create video resource\n");
        mrp_res_delete_resource_set(app_data->rs);
        return;
    }

    printf("created the resource set!\n");
}

void acquire_resources(my_app_data *app_data)
{
    /* acquire the resources */
    if (app_data->rs)
        mrp_res_acquire_resource_set(app_data->rs);
    else
        printf("No release set created!\n");
}

void giveup_resources(my_app_data *app_data)
{
    /* release resources */
    if (app_data->rs)
        mrp_res_release_resource_set(app_data->rs);
    else
        printf("No release set acquired!\n");
}

static void state_callback(mrp_res_context_t *context,
               mrp_res_error_t err,
               void *userdata)
{
    int i = 0, j = 0;
    const mrp_res_string_array_t *app_classes = NULL;
    const mrp_res_resource_set_t *rs;
    mrp_res_string_array_t *attributes = NULL;
    mrp_res_attribute_t *attr;
    bool system_handles_audio = FALSE;
    bool system_handles_video = FALSE;
    mrp_res_resource_t *resource;

    my_app_data *app_data = (my_app_data *) userdata;

    if (err != MRP_RES_ERROR_NONE) {
        printf("error message received from Murphy\n");
        return;
    }

    switch (context->state) {

        case MRP_RES_CONNECTED:

            printf("connected to murphy\n");

            if ((app_classes =
                        mrp_res_list_application_classes(context)) != NULL) {
                printf("listing all application classes in the system\n");

                for (i = 0; i < app_classes->num_strings; i++) {
                    printf("app class %d is %s\n", i, app_classes->strings[i]);
                }
            }

            if ((rs = mrp_res_list_resources(context)) != NULL) {
                mrp_res_string_array_t *resource_names;

                printf("listing all resources available in the system\n");

                resource_names = mrp_res_list_resource_names(rs);

                if (!resource_names) {
                    printf("No resources available in the system!\n");
                    return;
                }

                for (i = 0; i < resource_names->num_strings; i++) {

                    resource = mrp_res_get_resource_by_name(rs,
                            resource_names->strings[i]);

                    if (!resource)
                        continue;

                    printf("resource %d is %s\n", i, resource->name);
                    if (strcmp(resource->name, "audio_playback") == 0)
                        system_handles_audio = TRUE;
                    if (strcmp(resource->name, "video_playback") == 0)
                        system_handles_video = TRUE;

                    attributes = mrp_res_list_attribute_names(resource);

                    if (!attributes)
                        continue;

                    for (j = 0; j < attributes->num_strings; j++) {
                        attr = mrp_res_get_attribute_by_name(resource,
                                attributes->strings[j]);

                        if (!attr)
                            continue;

                        printf("attr %s has ", attr->name);
                        switch(attr->type) {
                            case mrp_string:
                                printf("type string and value %s\n",
                                        attr->string);
                                break;
                            case mrp_int32:
                                printf("type int32 and value %d\n",
                                        (int) attr->integer);
                                break;
                            case mrp_uint32:
                                printf("type uint32 and value %u\n",
                                        attr->unsignd);
                                break;
                            case mrp_double:
                                printf("type double and value %f\n",
                                        attr->floating);
                                break;
                            default:
                                printf("type unknown\n");
                                break;
                        }
                    }
                    mrp_res_free_string_array(attributes);
                }
                mrp_res_free_string_array(resource_names);
            }

            if (system_handles_audio && system_handles_video) {
                printf("system provides all necessary resources\n");
                accept_input = TRUE;
            }

            break;

        case MRP_RES_DISCONNECTED:
            printf("disconnected from murphy\n");
            mrp_res_delete_resource_set(app_data->rs);
            mrp_res_destroy(app_data->cx);
            exit(1);
    }
}


static char *state_to_str(mrp_res_resource_state_t st)
{
    char *state = "unknown";
    switch (st) {
        case MRP_RES_RESOURCE_ACQUIRED:
            state = "acquired";
            break;
        case MRP_RES_RESOURCE_LOST:
            state = "lost";
            break;
        case MRP_RES_RESOURCE_AVAILABLE:
            state = "available";
            break;
        case MRP_RES_RESOURCE_PENDING:
            state = "pending";
            break;
        case MRP_RES_RESOURCE_ABOUT_TO_LOOSE:
            state = "about to loose";
            break;
    }
    return state;
}

static void resource_callback(mrp_res_context_t *cx,
                  const mrp_res_resource_set_t *rs,
                  void *userdata)
{
    my_app_data *my_data = (my_app_data *) userdata;
    mrp_res_resource_t *res;
    mrp_res_attribute_t *attr;

    MRP_UNUSED(cx);

    printf("> resource_callback\n");

    if (!mrp_res_equal_resource_set(rs, my_data->rs))
        return;

    /* here compare the resource set difference */

    res = mrp_res_get_resource_by_name(rs, "audio_playback");

    if (!res) {
        printf("audio_playback not present in resource set\n");
        return;
    }

    printf("resource set state: %s\n", state_to_str(rs->state));

    printf("resource 0 name '%s' -> '%s'\n", res->name, state_to_str(res->state));

    res = mrp_res_get_resource_by_name(rs, "video_playback");

    if (!res) {
        printf("video_playback not present in resource set\n");
        return;
    }

    printf("resource 1 name '%s' -> '%s'\n", res->name, state_to_str(res->state));

    /* let's copy the changed set for ourselves */

    /* Delete must not mean releasing the set! Otherwise this won't work.
     * It's up to the user to make sure that there's a working reference
     * to the resource set.
     */
    mrp_res_delete_resource_set(my_data->rs);

    /* copying must also have no semantic meaning */
    my_data->rs = mrp_res_copy_resource_set(rs);

    /* print the current role attribute */

    res = mrp_res_get_resource_by_name(rs, "audio_playback");
    attr = mrp_res_get_attribute_by_name(res, "role");

    if (res && attr)
        printf("attribute '%s' has role '%s'\n", res->name, attr->string);

    /* acquiring a copy of an existing release set means:
     *  - acquired state: update, since otherwise no meaning
     *  - pending state: acquire, since previous state not known/meaningless
     *  - lost state: update, since otherwise will fail
     *  - available: update or acquire
     */
}

static void handle_input(mrp_io_watch_t *watch, int fd, mrp_io_event_t events,
                         void *user_data)
{
    mrp_mainloop_t *ml = mrp_get_io_watch_mainloop(watch);
    char buf[1024];
    int size;

    my_app_data *app_data = (my_app_data *) user_data;

    memset(buf, 0, sizeof(buf));

    if (events & MRP_IO_EVENT_IN) {
        size = read(fd, buf, sizeof(buf) - 1);

        if (size > 0) {
            buf[size] = '\0';
            printf("read line %s\n", buf);
        }
    }

    if (events & MRP_IO_EVENT_HUP) {
        mrp_del_io_watch(watch);
    }

    if (accept_input) {
        switch (buf[0]) {
            case 'C':
                create_resources(app_data);
                break;
            case 'A':
                acquire_resources(app_data);
                break;
            case 'D':
                giveup_resources(app_data);
                break;
            case 'Q':
                if (app_data->rs)
                    mrp_res_delete_resource_set(app_data->rs);
                if (ml)
                    mrp_mainloop_quit(ml, 0);
                break;
            default:
                printf("'C' to create resource set\n'A' to acquire\n'D' to release\n'Q' to quit\n");
                break;
       }
   }
   else {
       printf("not connected to Murphy\n");
   }
}

int main(int argc, char **argv)
{
    mrp_mainloop_t *ml;
    int mask;
    mrp_io_watch_t *watch;

    my_app_data app_data;

    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    if ((ml = mrp_mainloop_create()) == NULL)
        exit(1);

    app_data.rs = NULL;
    app_data.cx = mrp_res_create(ml, state_callback, &app_data);

    mask = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP | MRP_IO_EVENT_ERR;
    watch = mrp_add_io_watch(ml, fileno(stdin), (mrp_io_event_t) mask,
            handle_input, &app_data);

    if (!watch)
        exit(1);

    /* start looping */
    mrp_mainloop_run(ml);

    mrp_res_destroy(app_data.cx);
    mrp_mainloop_destroy(ml);

    app_data.cx = NULL;
    app_data.rs = NULL;

    return 0;
}
