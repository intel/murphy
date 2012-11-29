#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <murphy/plugins/resource-native/libmurphy-resource/resource-api.h>

typedef struct my_app_data {
    murphy_resource_context *cx;
    murphy_resource_set *rs;
} my_app_data;


static bool accept_input;

/* state callback for murphy connection */
static void state_callback(murphy_resource_context *cx,
			   murphy_resource_error,
			   void *ud);

/* callback for resource set update */
static void resource_callback(murphy_resource_context *cx,
			      const murphy_resource_set *rs,
			      void *userdata);

void acquire_resources(my_app_data *app_data)
{
    murphy_resource *resource= NULL;

    /* create resource set and resources */
    app_data->rs = murphy_resource_set_create(app_data->cx,
					      "player",
					      resource_callback,
					      (void*)app_data);

    if (app_data->rs == NULL) {
    	printf("Couldn't create resource set\n");
    	return;
    }

    resource = murphy_resource_create(app_data->cx,
				      app_data->rs,
				      "audio_playback",
				      true,
				      false);

    if (resource == NULL) {
    	printf("Couldn't create audio resource\n");
    	murphy_resource_set_delete(app_data->rs);
    	return;
    }

    resource = murphy_resource_create(app_data->cx,
				      app_data->rs,
				      "video_playback",
				      true,
				      false);


    if (resource == NULL) {
    	printf("Couldn't create video resource\n");
    	murphy_resource_set_delete(app_data->rs);
    	return;
    }

    /* acquire the resources */
    murphy_resource_set_acquire(app_data->cx, app_data->rs);
}

void giveup_resources(my_app_data *app_data)
{
    printf("giving up resources\n");

    /* release resources */
    murphy_resource_set_release(app_data->cx, app_data->rs);
}

static void state_callback(murphy_resource_context *context,
			   murphy_resource_error err,
			   void *userdata)
{
    int i = 0, j = 0;
    const murphy_string_array *app_classes = NULL;
    const murphy_resource_set *rs;
    murphy_string_array *attributes = NULL;
    murphy_resource_attribute *attr;
    bool system_handles_audio = FALSE;
    bool system_handles_video = FALSE;

    my_app_data *app_data = userdata;

    if (err != murphy_resource_error_none) {
        printf("error message received from Murphy\n");
        return;
    }

    switch (context->state) {

        case murphy_connected:
            printf("connected to murphy\n");

            if ((app_classes = murphy_application_class_list(context)) != NULL) {
                printf("listing all application classes in the system\n");

                for (i = 0; i < app_classes->num_strings; i++) {
                    printf("app class %d is %s\n", i, app_classes->strings[i]);
                }
            }

            if ((rs = murphy_resource_set_list(context)) != NULL) {
                murphy_string_array *resource_names;

                printf("listing all resources available in the system\n");

                resource_names = murphy_resource_list_names(context, rs);

                for (i = 0; i < resource_names->num_strings; i++) {

                    murphy_resource *resource;

                    resource = murphy_resource_get_by_name(context, rs,
                            resource_names->strings[i]);

                    printf("resource %d is %s\n", i, resource->name);
                    if (strcmp(resource->name, "audio_playback") == 0)
                        system_handles_audio = TRUE;
                    if (strcmp(resource->name, "video_playback") == 0)
                        system_handles_video = TRUE;

                    attributes = murphy_attribute_list_names(context, resource);

                    for (j = 0; j < attributes->num_strings; j++) {
                        attr = murphy_attribute_get_by_name(context,
                                resource,
                                attributes->strings[i]);
                        printf("attr %s has ", attr->name);
                        switch(attr->type) {
                            case murphy_string:
                                printf("type string and value %s\n", attr->string);
                                break;
                            case murphy_int32:
                                printf("type string and value %d\n", attr->integer);
                                break;
                            case murphy_uint32:
                                printf("type string and value %u\n", attr->unsignd);
                                break;
                            case murphy_double:
                                printf("type string and value %f\n", attr->floating);
                                break;
                            default:
                                printf("type unknown\n");
                                break;
                        }

                    }
                }
            }

            if (system_handles_audio && system_handles_video) {
                printf("system provides all necessary resources\n");
                accept_input = TRUE;
            }

            break;

        case murphy_disconnected:
            printf("disconnected from murphy\n");
            murphy_resource_set_delete(app_data->rs);
            murphy_destroy(app_data->cx);
            exit(1);
    }
}

static void resource_callback(murphy_resource_context *cx,
			      const murphy_resource_set *rs,
			      void *userdata)
{
    my_app_data *my_data = (my_app_data *) userdata;
    murphy_resource *res;

    if (!murphy_resource_set_equals(rs, my_data->rs))
        return;

    /* here compare the resource set difference */

    res = murphy_resource_get_by_name(cx, rs, "audio_playback");

    if (!res) {
        printf("audio_playback not present in resource set");
        return;
    }

    printf("resource 0 name %s\n", res->name);
    printf("resource 0 state %d\n", res->state);

    res = murphy_resource_get_by_name(cx, rs, "video_playback");

    if (!res) {
        printf("video_playback not present in resource set");
        return;
    }

    printf("resource 1 name %s\n", res->name);
    printf("resource 1 state %d\n", res->state);

    /* let's copy the changed set for ourselves */

    /* Delete must not mean releasing the set! Otherwise this won't work.
     * It's up to the user to make sure that there's a working reference
     * to the resource set.
     */
    murphy_resource_set_delete(my_data->rs);

    /* copying must also have no semantic meaning */
    my_data->rs = murphy_resource_set_copy(rs);


    /* acquiring a copy of an existing release set means:
     *  - acquired state: update, since otherwise no meaning
     *  - pending state: acquire, since previous state not known/meaningless
     *  - lost state: update, since otherwise will fail
     *  - available: update or acquire
     */

}

static void handle_input(mrp_mainloop_t *ml, mrp_io_watch_t *watch, int fd,
                    mrp_io_event_t events, void *user_data)
{
    char buf[1024];
    int size;

    my_app_data *app_data = user_data;

    MRP_UNUSED(ml);

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
            case 'C':  acquire_resources(app_data);
                break;
            case 'D':  giveup_resources(app_data);
                break;
            default:
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
    mrp_io_event_t mask;
    mrp_io_watch_t *watch;

    my_app_data app_data;

    if ((ml = mrp_mainloop_create()) == NULL)
        exit(1);

    app_data.cx = murphy_create(ml, state_callback, &app_data);

    mask = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP | MRP_IO_EVENT_ERR;
    watch = mrp_add_io_watch(ml, fileno(stdin), mask, handle_input, &app_data);

    mrp_mainloop_run(ml);

    return 0;
}
