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
static void state_callback(murphy_resource_context *cx, enum murphy_resource_error, void *ud);
/* callback for resource set update */
static void resource_callback(murphy_resource_context *cx,
            murphy_resource_set *rs, void *userdata);

void acquire_resources(my_app_data *app_data)
{
    murphy_resource *resource;

    /* create resources and add them to the resource set */
    app_data->rs = murphy_create_resource_set("player");
    murphy_set_resource_set_callback(app_data->rs, resource_callback, app_data);

    resource = murphy_resource_create(app_data->cx, "audio_playback", true, false);
    murphy_add_resource(app_data->rs, resource);

    resource = murphy_resource_create(app_data->cx, "video_playback", true, false);
    murphy_add_resource(app_data->rs, resource);

    /* identify ourselves, ask for the resource set and assign resource callback */
    murphy_acquire_resources(app_data->cx, app_data->rs);
}

void giveup_resources(my_app_data *app_data)
{
    int i = 0;

    printf("giving up resources\n");

    /* set all resource states to lost -> give up resource */
    for (i = 0; i < app_data->rs->num_resources; i++)
        app_data->rs->resources[i]->state = murphy_resource_lost;

    /* identify ourselves, ask for the resource set and assing resource callback */
    murphy_release_resources(app_data->cx, app_data->rs);
}

static void state_callback(murphy_resource_context *context,
            enum murphy_resource_error err, void *userdata)
{
    int i = 0;
    const char **app_classes = NULL;
    int num_classes;
    murphy_resource_set *rs;
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

            if (murphy_list_application_classes(context, &app_classes, &num_classes) >= 0) {
                for (i = 0; i < num_classes; i++) {
                    printf("app class %d is %s\n", i, app_classes[i]);
                }
#if 0
                for (i = 0; i < num_classes; i++) {
                    free(app_classes[i]);
                }
                free(app_classes);
#endif
            }

            murphy_list_resources(context, &rs);

            printf("listing all resources available in the system\n");
            for (i = 0; i < rs->num_resources; i++) {
                printf("resource %d is %s\n", i, rs->resources[i]->name);
                if (strcmp(rs->resources[i]->name, "audio_playback") == 0)
                    system_handles_audio = TRUE;
                if (strcmp(rs->resources[i]->name, "video_playback") == 0)
                    system_handles_video = TRUE;
            }

            if (system_handles_audio && system_handles_video) {
                printf("system provides all necessary resources\n");
                accept_input = TRUE;
            }
            murphy_delete_resource_set(rs);

            break;

        case murphy_disconnected:
            printf("disconnected from murphy\n");
            murphy_delete_resource_set(app_data->rs);
            murphy_destroy(app_data->cx);
            exit(1);
    }
}

static void resource_callback(murphy_resource_context *cx,
            murphy_resource_set *rs, void *userdata)
{
    my_app_data *my_data = (my_app_data *) userdata;
    int i;

    if (!murphy_same_resource_sets(rs, my_data->rs))
        return;

    for (i = 0; i < rs->num_resources; i++) {
        /* here compare the resource set difference */
        printf("resource %d name %s\n", i, rs->resources[i]->name);
        printf("resource %d state %d\n", i, rs->resources[i]->state);
    }

    /* let's copy the changed set for ourselves */

    /* Delete must not mean releasing the set! Otherwise this won't work.
     * It's up to the user to make sure that there's a working reference
     * to the resource set.
     */
    murphy_delete_resource_set(my_data->rs);

    /* copying must also have no semantic meaning */
    my_data->rs = murphy_copy_resource_set(rs);


    /* acquiring a copy of an existing release set means:
     *  - acquired state: update, since otherwise no meaning
     *  - pending state: acquire, since previous state not known/meaningless
     *  - lost state: update, since otherwise will fail
     *  - available: update or acquire
     */

}

#if 0
static gboolean handle_input(GIOChannel *ch, GIOCondition cond, gpointer data)
{
    char cmd[5];

    (void)ch;
    (void)cond;
    (void)data;

    if (fgets(cmd, 2, stdin) == NULL) {
       printf("input error\n");
       return FALSE;
   }

   if (accept_input) {
       switch (cmd[0]) {
           case 'C':  acquire_resources();              break;
           case 'D':  giveup_resources();               break;
           default:                                     break;
       }
   }
   else {
       printf("not connected to Murphy\n");
   }

   return TRUE;
}
#endif

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
