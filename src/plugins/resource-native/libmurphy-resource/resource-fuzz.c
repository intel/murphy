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

#define DEFAULT_SEED 101

static unsigned int seed;

typedef struct {
    mrp_res_resource_set_t *rset;
    bool acquired;
    mrp_list_hook_t hook;
} rset_item_t;

typedef struct {
    /* resource context */
    mrp_res_context_t *cx;

    /* iteration handling */
    mrp_mainloop_t *ml;
    mrp_deferred_t *d;
    unsigned int iterations_left;

    /* resource set list */
    mrp_list_hook_t rsets;
    int n_items;
} fuzz_data_t;

enum operation_e {
    OP_CREATE = 0,
    OP_DELETE,
    OP_ACQUIRE,
    OP_RELEASE,
    OP_MAX
} operations;


static void next_seed() {
    char tmp_array[9];
    char buf[7];
    int offset;
    int len;
    int i;
    unsigned int tmp = seed * seed;

    /* Von Neumann's algorithm */

    snprintf(tmp_array, 8, "%u", tmp);

    len = strlen(tmp_array);
    offset = 8 - len;

    /* move to the end of the buffer*/
    memmove(tmp_array+offset, tmp_array, len);

    for (i = 0; i < offset; i++) {
        tmp_array[i] = ' '; /* space before the num */
    }

    buf[6] = '\0';

    /* move the last three bytes */

    buf[5] = tmp_array[7];
    buf[4] = tmp_array[6];
    buf[3] = tmp_array[5];

    /* the first three bytes */

    buf[2] = tmp_array[2];
    buf[1] = tmp_array[1];
    buf[0] = tmp_array[0];

    seed = strtoul(buf, NULL, 10);

    /* printf("new seed: %u\n", seed); */
}

static int get_index(int max)
{
    next_seed();
    return seed % max;
}

static void shuffle_strings(char **strings, int n) {
    char **p; /* non-shuffled strings */
    char *swap;
    int i, rem, idx;

    /* in place shuffle */

    for (i = 0, rem = n; i < n; i++, rem--) {
        p = strings+i;

        idx = get_index(rem);

        /* swap the elements */
        swap = p[idx];
        p[idx] = strings[i];
        strings[i] = swap;
    }
}

static void resource_callback(mrp_res_context_t *cx,
                  const mrp_res_resource_set_t *rs,
                  void *userdata)
{
    MRP_UNUSED(cx);
    MRP_UNUSED(rs);
    MRP_UNUSED(userdata);

    return;
}

static void acquire_rset(fuzz_data_t *data)
{
    int i;
    mrp_list_hook_t *ip, *in;
    rset_item_t *item = NULL;

    if (data->n_items == 0)
        goto error;

    i = get_index(data->n_items);

    mrp_list_foreach(&data->rsets, ip, in) {
        item = mrp_list_entry(ip, rset_item_t, hook);

        if (i-- == 0) {
            if (!item->acquired) {
                mrp_res_acquire_resource_set(item->rset);
                item->acquired = TRUE;
            }
            return;
        }
    }

error:
    return;
}

static void release_rset(fuzz_data_t *data)
{
    int i;
    mrp_list_hook_t *ip, *in;
    rset_item_t *item = NULL;

    if (data->n_items == 0)
        goto error;

    i = get_index(data->n_items);

    mrp_list_foreach(&data->rsets, ip, in) {
        item = mrp_list_entry(ip, rset_item_t, hook);

        if (i-- == 0) {
            if (item->acquired) {
                mrp_res_release_resource_set(item->rset);
                item->acquired = FALSE;
            }
            return;
        }
    }

error:
    return;
}

static void delete_rset(fuzz_data_t *data)
{
    int i;
    mrp_list_hook_t *ip, *in;
    rset_item_t *item = NULL;

    if (data->n_items == 0)
        goto error;

    i = get_index(data->n_items);

    mrp_list_foreach(&data->rsets, ip, in) {
        item = mrp_list_entry(ip, rset_item_t, hook);

        if (i-- == 0) {
            mrp_list_delete(ip);
            mrp_res_delete_resource_set(item->rset);
            mrp_free(item);
            data->n_items--;
            return;
        }
    }

error:
    return;
}

static void create_resource(mrp_res_resource_set_t *rset, char *resource)
{
    mrp_res_resource_t *res;
    bool attrs[][2] = { {TRUE, TRUE}, {TRUE, FALSE}, {FALSE, TRUE}, {FALSE, FALSE} };

    bool *attr = attrs[get_index(4)];

    res = mrp_res_create_resource(rset, resource, attr[0], attr[1]);

    /* TODO: set some attributes */

    MRP_UNUSED(res);

    return;
}

static void create_rset(fuzz_data_t *data)
{
    mrp_res_resource_set_t *rset;
    rset_item_t *item;

    char *app_classes[] = { "player", "game", "navigator" };
    char *app_class = app_classes[get_index(3)];

    char *resources[] = { "audio_playback", "audio_recording" };
    int n_resources = get_index(1) + 1;
    int i;

    item = (rset_item_t *) mrp_allocz(sizeof(rset_item_t));

    if (!item)
        goto error;

    mrp_list_init(&item->hook);

    rset = mrp_res_create_resource_set(data->cx,
            app_class, resource_callback, data);

    if (!rset)
        goto error;

    /* create resources */

    shuffle_strings(resources, 2);

    for (i = 0; i < n_resources; i++) {
        create_resource(rset, resources[i]);
    }

    /* put the resource set to the rset list */

    item->rset = rset;
    item->acquired = FALSE;

    mrp_list_append(&data->rsets, &item->hook);

    data->n_items++;

    return;

error:
    return;
}

static void fuzz_iteration(mrp_deferred_t *d, void *user_data)
{
    enum operation_e op = (enum operation_e) get_index(OP_MAX);

    fuzz_data_t *data =  (fuzz_data_t *) user_data;

    data->iterations_left--;

    if (data->iterations_left == 0)
        mrp_disable_deferred(d);

#if 1
    printf("iterations left: %d, operation: %d\n", data->iterations_left,
           op);
#endif

    switch (op) {
        case OP_CREATE:
            create_rset(data);
            break;
        case OP_DELETE:
            delete_rset(data);
            break;
        case OP_ACQUIRE:
            acquire_rset(data);
            break;
        case OP_RELEASE:
            release_rset(data);
            break;
        default:
            break;
    }
}


static void state_callback(mrp_res_context_t *context,
               mrp_res_error_t err,
               void *userdata)
{
    fuzz_data_t *data = (fuzz_data_t *) userdata;

    if (err != MRP_RES_ERROR_NONE) {
        printf("error message received from Murphy\n");
        goto error;
    }

    switch (context->state) {
        case MRP_RES_CONNECTED:
            data->d = mrp_add_deferred(data->ml, fuzz_iteration, data);
            if (!data->d) {
                printf("Error creating iteration loop\n");
                goto error;
            }
            break;
        case MRP_RES_DISCONNECTED:
            if (data->d)
                mrp_del_deferred(data->d);
            goto error;
    }
    return;

error:
    /* TODO (for memory analysis reasons) */
    /* exit(1); */
    return;
}

static void usage()
{
    printf("Usage:\n");
    printf("\tresource-api-fuzz <iterations> [seed]\n");
}

int main(int argc, char **argv)
{
    mrp_mainloop_t *ml;
    fuzz_data_t data;

    memset(&data, 0, sizeof(fuzz_data_t));

    if (argc < 2) {
        usage();
        exit(1);
    }

    if (argc >= 3)
        seed = strtoul(argv[2], NULL, 10);
    else
        seed = DEFAULT_SEED;

    if ((ml = mrp_mainloop_create()) == NULL)
        exit(1);

    data.cx = mrp_res_create(ml, state_callback, &data);
    data.ml = ml;
    data.iterations_left = strtoul(argv[1], NULL, 10);
    mrp_list_init(&data.rsets);

    /* start looping */
    mrp_mainloop_run(ml);

    mrp_res_destroy(data.cx);
    mrp_mainloop_destroy(ml);

    data.cx = NULL;

    return 0;
}
