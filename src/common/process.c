/*
 * Copyright (c) 2013, Intel Corporation
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
#include <unistd.h>
#include <errno.h>
#include <sys/inotify.h>

#include <murphy/common/process.h>
#include <murphy/common.h>

#define MURPHY_PROCESS_INOTIFY_DIR "/var/run/murphy/processes"

typedef struct {
    char *path;
    mrp_process_watch_handler_t cb;
    void *userdata;
    mrp_io_watch_t *inotify_cb;
} i_watch_t;

static int i_fd;
static int dir_fd;
static mrp_htbl_t *i_watches;
static int i_n_watches;
static mrp_io_watch_t *i_wd;


static bool id_ok(const char *id)
{
    int i, len;
    /* restrict the input */

    len = strlen(id);

    for (i = 0; i < len; i++) {
        bool character, number, special;

        character = ((id[i] >= 'A' && id[i] <= 'Z') ||
                    (id[i] >= 'a' && id[i] <= 'z'));

        number = (id[i] >= '0' && id[i] <= '9');

        special = (id[i] == '-' || id[i] == '_');

        if (!(character || number || special))
            return FALSE;
    }

    return TRUE;
}


static char *path_from_id(const char *id)
{
    char buf[PATH_MAX];
    int ret;

    if (!id || !id_ok(id))
        return NULL;

    ret = snprintf(buf, PATH_MAX, "%s/%s", MURPHY_PROCESS_INOTIFY_DIR, id);

    if (ret < 0 || ret == PATH_MAX)
        return NULL;

    return mrp_strdup(buf);
}


static void htbl_free_i_watch(void *key, void *object)
{
    i_watch_t *w = (i_watch_t *) object;

    MRP_UNUSED(key);

    mrp_free(w->path);
    mrp_free(w);
}


static int initialize_dir()
{
    /* TODO: check if the directory is present; if not, create it */

    return 0;
}


static void process_change(mrp_mainloop_t *ml, mrp_io_watch_t *wd, int fd,
        mrp_io_event_t events, void *user_data)
{
    struct inotify_event *is;
    int bufsize = sizeof(struct inotify_event) + PATH_MAX;
    char buf[bufsize];
    i_watch_t *w;
    FILE *f;

    MRP_UNUSED(ml);
    MRP_UNUSED(wd);
    MRP_UNUSED(user_data);

    if (events & MRP_IO_EVENT_IN) {
        char *path = NULL;
        int read_bytes;
        int processed_bytes = 0;

        read_bytes = read(fd, buf, bufsize);

        if (read_bytes < 0) {
            mrp_log_error("Failed to read event from inotify: %s",
                    strerror(errno));
            return;
        }

        do {
            /* the kernel doesn't allow to read incomplete events */

            is = (struct inotify_event *) (buf + processed_bytes);

            processed_bytes = sizeof(struct inotify_event) + is->len;

            if (!i_watches)
                return;

            w = (i_watch_t *) mrp_htbl_lookup(i_watches, is->name);

            if (!w) {
                /* this event wasn't for us */
                continue;
            }

            path = path_from_id(w->path);

            if (!path) {
                continue;
            }

            f = fopen(path, "r");

            if (f) {
                fclose(f);
                mrp_log_info("Received inotify event for %s, READY", w->path);
                w->cb(w->path, MRP_PROCESS_STATE_READY, w->userdata);
            }
            else {
                mrp_log_info("Received inotify event for %s, NOT READY", w->path);
                w->cb(w->path, MRP_PROCESS_STATE_NOT_READY, w->userdata);
            }

            mrp_free(path);

        } while (processed_bytes < read_bytes);
    }
}


static int initialize(mrp_mainloop_t *ml)
{
    if (initialize_dir() < 0)
        goto error;

    if (i_fd <= 0) {
        i_fd = inotify_init();

        if (i_fd <= 0)
            goto error;
    }

    if (dir_fd <= 0) {

        dir_fd = inotify_add_watch(i_fd, MURPHY_PROCESS_INOTIFY_DIR,
                IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_MODIFY);

        if (dir_fd < 0)
            goto error;

        i_wd = mrp_add_io_watch(ml, i_fd, MRP_IO_EVENT_IN, process_change, NULL);

        if (!i_wd) {
            inotify_rm_watch(i_fd, dir_fd);
            goto error;
        }
    }

    if (!i_watches) {
        mrp_htbl_config_t watches_conf;

        watches_conf.comp = mrp_string_comp;
        watches_conf.hash = mrp_string_hash;
        watches_conf.free = htbl_free_i_watch;
        watches_conf.nbucket = 0;
        watches_conf.nentry = 10;

        i_watches = mrp_htbl_create(&watches_conf);

        if (!i_watches)
            goto error;
    }

    return 0;

error:

    if (i_watches) {
        mrp_htbl_destroy(i_watches, FALSE);
        i_watches = NULL;
    }


    if (i_wd) {
        mrp_del_io_watch(i_wd);
        i_wd = NULL;
    }

    if (i_fd && dir_fd) {
        inotify_rm_watch(i_fd, dir_fd);
        dir_fd = -1;
    }

    i_n_watches = 0;

    return -1;
}


int mrp_process_set_state(const char *id, mrp_process_state_t state)
{
    char *path = NULL;
    FILE *f;
    int ret = -1;

    if (!initialize_dir() < 0)
        goto end;

    path = path_from_id(id);

    if (!path)
        goto end;

    switch (state) {
        case MRP_PROCESS_STATE_UNKNOWN:
        case MRP_PROCESS_STATE_NOT_READY:
            if (unlink(path) < 0) {
                if (errno != ENOENT) {
                    goto end;
                }
            }
            break;
        case MRP_PROCESS_STATE_READY:
            f = fopen(path, "w");
            if (!f)
                goto end;

            fclose(f);
            break;
    }

    ret = 0;

end:
    mrp_free(path);
    return ret;
}


mrp_process_state_t mrp_process_query_state(const char *id)
{
    char *path;
    FILE *f;

    if (initialize_dir() < 0)
        return MRP_PROCESS_STATE_UNKNOWN;

    path = path_from_id(id);

    if (!path)
        return MRP_PROCESS_STATE_UNKNOWN;

    f = fopen(path, "r");

    mrp_free(path);

    if (f) {
        fclose(f);
        return MRP_PROCESS_STATE_READY;
    }

    return MRP_PROCESS_STATE_NOT_READY;
}


int mrp_process_set_watch(const char *id, mrp_mainloop_t *ml,
        mrp_process_watch_handler_t cb, void *userdata)
{
    i_watch_t *w = NULL;

    if (initialize(ml) < 0)
        goto error;

    w = (i_watch_t *) mrp_alloc(sizeof(i_watch_t));

    if (!w)
        goto error;

    w->path = mrp_strdup(id);
    w->cb = cb;
    w->userdata = userdata;
    w->inotify_cb = i_wd;

    if (mrp_htbl_insert(i_watches, w->path, w) < 0)
        goto error;

    i_n_watches++;

    return 0;

error:
    mrp_free(w);

    return -1;
}


int mrp_process_remove_watch(const char *id)
{
    i_watch_t *w;

    if (!i_watches)
        return -1;

    w = (i_watch_t *) mrp_htbl_lookup(i_watches, (void *)id);

    if (!w)
        return -1;

    mrp_htbl_remove(i_watches, (void *) id, TRUE);
    i_n_watches--;

    if (i_n_watches <= 0) {
        /* TODO: if the map is empty, destroy it and NULL the pointer? */

        mrp_htbl_destroy(i_watches, TRUE);
        i_watches = NULL;

        i_n_watches = 0;

        if (i_fd > 0 && dir_fd > 0) {
           inotify_rm_watch(i_fd, dir_fd);
           dir_fd = -1;
        }
    }

    return 0;
}
