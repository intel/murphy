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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <linux/cn_proc.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <linux/filter.h>

#include <murphy/common/process.h>
#include <murphy/common.h>


#define MURPHY_PROCESS_INOTIFY_DIR "/var/run/murphy/processes"

struct mrp_pid_watch_s {
    pid_t pid;
};

typedef struct {
    char *path; /* file name token */
    pid_t pid;
    char *filename;

    /* either this ... */
    mrp_process_watch_handler_t process_cb;
    /* ... or this is set. */
    mrp_pid_watch_handler_t pid_cb;

    void *userdata;
    mrp_io_watch_t *inotify_cb;
} i_watch_t;

typedef struct {
    mrp_pid_watch_handler_t cb;
    void *user_data;
    mrp_pid_watch_t *w; /* identify the client */

    mrp_list_hook_t hook;
} nl_pid_client_t;

typedef struct {
    pid_t pid;
    char pid_s[16]; /* memory for hashing */

    mrp_list_hook_t clients;
    int n_clients;
    int busy : 1;
    int dead : 1;
} nl_pid_watch_t;

/* murphy pid file directory notify */
static int dir_fd;
static int i_n_process_watches;

/* inotify */
static int i_fd;
static mrp_htbl_t *i_watches;
static mrp_io_watch_t *i_wd;

/* netlink process listening */
static int nl_sock;
static bool subscribed;
static mrp_io_watch_t *nl_wd;
static mrp_htbl_t *nl_watches;
static int nl_n_pid_watches;

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

    if (ret < 0 || ret >= PATH_MAX)
        return NULL;

    return mrp_strdup(buf);
}


static void htbl_free_nl_watch(void *key, void *object)
{
    nl_pid_watch_t *w = (nl_pid_watch_t *) object;

    MRP_UNUSED(key);

    if (!w->busy)
        mrp_free(w);
    else
        w->dead = TRUE;
}


static void htbl_free_i_watch(void *key, void *object)
{
    i_watch_t *w = (i_watch_t *) object;

    MRP_UNUSED(key);

    mrp_free(w->path);
    mrp_free(w->filename);
    mrp_free(w);
}


static int initialize_dir()
{
    /* TODO: check if the directory is present; if not, create it */

    return 0;
}


static void process_change(mrp_io_watch_t *wd, int fd, mrp_io_event_t events,
                           void *user_data)
{
    struct inotify_event *is;
    int bufsize = sizeof(struct inotify_event) + PATH_MAX + 1;
    char buf[bufsize];
    i_watch_t *w;
    FILE *f;

    MRP_UNUSED(wd);
    MRP_UNUSED(user_data);

    if (!i_watches)
        return;

    if (events & MRP_IO_EVENT_IN) {
        int read_bytes;
        int processed_bytes = 0;

        read_bytes = read(fd, buf, bufsize - 1);

        if (read_bytes < 0) {
            mrp_log_error("Failed to read event from inotify: %s",
                    strerror(errno));
            return;
        }

        buf[read_bytes] = '\0';

        while (processed_bytes < read_bytes) {
            char *filename = NULL;

            /* the kernel doesn't allow to read incomplete events */
            is = (struct inotify_event *) (buf + processed_bytes);

            processed_bytes += sizeof(struct inotify_event) + is->len;

            if (is->len == 0) {
                /* no file name */
                continue;
            }

            if (is->wd != dir_fd) {
                /* wrong descriptor? */
                continue;
            }

            filename = path_from_id(is->name);

            if (!filename)
                continue;

            w = (i_watch_t *) mrp_htbl_lookup(i_watches, filename);

            if (w) {
                f = fopen(filename, "r");

                if (f) {
                    fclose(f);
                    mrp_log_info("Received inotify event for %s, READY", w->path);
                    w->process_cb(w->path, MRP_PROCESS_STATE_READY, w->userdata);
                }
                else {
                    mrp_log_info("Received inotify event for %s, NOT READY", w->path);
                    w->process_cb(w->path, MRP_PROCESS_STATE_NOT_READY, w->userdata);
                }
            }
            mrp_free(filename);
        }
    }
}


static int send_proc_cmd(enum proc_cn_mcast_op cmd)
{
    int data_size = sizeof(enum proc_cn_mcast_op);

    /* connector message size */
    int cn_size = sizeof(struct cn_msg);

    /* total size of bytes we need */
    int message_size = NLMSG_SPACE(cn_size + data_size);

    /* aligned size */
    int payload_size = NLMSG_LENGTH(message_size - sizeof(struct nlmsghdr));

    /* helper pointers */
    struct nlmsghdr *nl;
    struct cn_msg *cn;

    /* message data */
    char buf[message_size];

    if (nl_sock <= 0) {
        mrp_log_error("invalid netlink socket %d", nl_sock);
        return -1;
    }

    /* structs point to the aligned memory */
    nl = (struct nlmsghdr *) buf;
    cn = (struct cn_msg *) NLMSG_DATA(nl);

    /* fill the structures */
    nl->nlmsg_len = payload_size;
    nl->nlmsg_seq = 0;
    nl->nlmsg_pid = getpid();
    nl->nlmsg_type = NLMSG_DONE;
    nl->nlmsg_flags = 0;

    cn->len = data_size;
    cn->seq = 0;
    cn->ack = 0;
    cn->id.idx = CN_IDX_PROC;
    cn->id.val = CN_VAL_PROC;
    cn->flags = 0;

    /* all this was done for this data */
    memcpy(cn->data, &cmd, data_size);

    return send(nl_sock, buf, message_size, 0);
}


static int subscribe_proc_events()
{
    int ret = send_proc_cmd(PROC_CN_MCAST_LISTEN);

    if (ret >= 0)
        subscribed = TRUE;

    return ret;
}


static int unsubscribe_proc_events()
{
    int ret = send_proc_cmd(PROC_CN_MCAST_IGNORE);

    if (ret >= 0)
        subscribed = FALSE;

    return ret;
}


static void nl_watch(mrp_io_watch_t *w, int fd, mrp_io_event_t events,
        void *user_data)
{
    char buf[4096];
    struct nlmsghdr *nl;
    struct sockaddr_nl addr;
    unsigned int sockaddr_len;
    ssize_t len;

    MRP_UNUSED(w);
    MRP_UNUSED(events);
    MRP_UNUSED(user_data);

    sockaddr_len = sizeof(struct sockaddr_nl);

    len = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *) &addr,
            &sockaddr_len);

    if (len < 0) {
        mrp_log_error("failed to read data from socket");
        return;
    }

    if (addr.nl_pid != 0) {
        mrp_log_error("message wasn't from the kernel");
        return;
    }

    /* set pointer to the first message in the buffer */
    nl = (struct nlmsghdr *) buf;

    while (NLMSG_OK(nl, (unsigned int) len)) {
        struct cn_msg *cn;

        /* we are expecting a non-multipart message -- filter errors and
         * others away */
        if (nl->nlmsg_type != NLMSG_DONE) {
            if (nl->nlmsg_type == NLMSG_ERROR) {
                /* TODO: error processing, resynchronization */
            }
            nl = NLMSG_NEXT(nl, len);
            continue;
        }

        cn = (struct cn_msg *) NLMSG_DATA(nl);

        if (cn->id.idx == CN_IDX_PROC && cn->id.val == CN_VAL_PROC) {
            struct proc_event *ev = (struct proc_event *) cn->data;

            switch (ev->what) {
                case PROC_EVENT_EXIT:
                {
                    mrp_list_hook_t *p, *n;
                    nl_pid_watch_t *nl_w;
                    char pid_s[16];
                    int ret;

                    mrp_log_info("process %d exited",
                            ev->event_data.exit.process_pid);

                    ret = snprintf(pid_s, sizeof(pid_s), "%u",
                            (unsigned int) ev->event_data.exit.process_pid);

                    if (ret < 0 || ret >= (int) sizeof(pid_s))
                        break;

                    /* check the pid */
                    nl_w = (nl_pid_watch_t *) mrp_htbl_lookup(nl_watches, pid_s);

                    if (!nl_w) {
                        mrp_log_error("pid %s exited but no-one was following it", pid_s);
                        break;
                    }

                    nl_w->busy = TRUE;
                    mrp_list_foreach(&nl_w->clients, p, n) {
                        nl_pid_client_t *client;

                        client = mrp_list_entry(p, typeof(*client), hook);
                        client->cb(nl_w->pid, MRP_PROCESS_STATE_NOT_READY,
                                client->user_data);
                    }
                    if (nl_w->dead)
                        mrp_free(nl_w);
                    else
                        nl_w->busy = FALSE;

                    /* TODO: should we automatically free the wathces? Or let
                     * client do that to preserver symmetricity? */
                    break;
                }
                default:
                    /* the filter isn't working for some reason */
                    mrp_log_error("some other message!\n");
                    break;
            }
        }

        nl = NLMSG_NEXT(nl, len);
    }
}


static int initialize(mrp_mainloop_t *ml, bool process, bool pid)
{
    if (process) {

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
        }

        if (!i_wd) {
            i_wd = mrp_add_io_watch(ml, i_fd, MRP_IO_EVENT_IN, process_change, NULL);

            if (!i_wd)
                goto error;
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
    }

    if (pid) {
        if (nl_sock <= 0) {
            struct sockaddr_nl nl_addr;
            int nl_options = SOCK_NONBLOCK | SOCK_DGRAM | SOCK_CLOEXEC;
            struct sock_filter block[] = {
                BPF_STMT(BPF_RET | BPF_K, 0x0),
            };
            struct sock_fprog fp;

            /* socket creation */

            nl_sock = socket(PF_NETLINK, nl_options, NETLINK_CONNECTOR);

            if (nl_sock <= 0)
                goto error;

            memset(&nl_addr, 0, sizeof(struct sockaddr_nl));
            memset(&fp, 0, sizeof(struct sock_fprog));

            /* bind the socket to the address */

            nl_addr.nl_pid = getpid();
            nl_addr.nl_family = AF_NETLINK;
            nl_addr.nl_groups = CN_IDX_PROC;

            if (bind(nl_sock, (struct sockaddr *) &nl_addr,
                    sizeof(struct sockaddr_nl)) < 0)
                goto error;

            fp.filter = block;
            fp.len = 1;

            /* set socket filter that blocks everything */
            if (setsockopt(nl_sock, SOL_SOCKET, SO_ATTACH_FILTER, &fp,
                    sizeof(struct sock_fprog)) < 0) {
                mrp_log_error("setting blocking socket filter failed: %s",
                        strerror(errno));
                goto error;
            }

            nl_wd = mrp_add_io_watch(ml, nl_sock, MRP_IO_EVENT_IN, nl_watch, NULL);
        }

        if (!nl_watches) {
            mrp_htbl_config_t watches_conf;

            watches_conf.comp = mrp_string_comp;
            watches_conf.hash = mrp_string_hash;
            watches_conf.free = htbl_free_nl_watch;
            watches_conf.nbucket = 0;
            watches_conf.nentry = 10;

            nl_watches = mrp_htbl_create(&watches_conf);

            if (!nl_watches)
                goto error;
        }
    }

    return 0;

error:
    mrp_log_error("initialization error");

    if (process) {

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

        i_n_process_watches = 0;
    }

    if (pid) {

        if (nl_sock > 0) {
            close(nl_sock);
            nl_sock = -1;
        }

        nl_n_pid_watches = 0;
    }

    return -1;
}


static int initialize_process(mrp_mainloop_t *ml)
{
    return initialize(ml, TRUE, FALSE);
}


static int initialize_pid(mrp_mainloop_t *ml)
{
    return initialize(ml, FALSE, TRUE);
}


int mrp_process_set_state(const char *id, mrp_process_state_t state)
{
    char *path = NULL;
    FILE *f;
    int ret = -1;

    if (initialize_dir() < 0)
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


static void filter_add_pid(struct sock_filter *p, pid_t pid, int proc_offset)
{
    int proc_event_data_offset = proc_offset +
            offsetof(struct proc_event, event_data);
    /* event_data is an union, leaving out */
    int proc_event_data_exit_pid_offset = proc_event_data_offset +
            offsetof(struct exit_proc_event, process_pid);

    struct sock_filter bpf[] = {
        /* load the pid value ... */
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, proc_event_data_exit_pid_offset),

        /* ... if it is the pid we're comparing it to ... */
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, htonl(pid), 0, 1),

        /* ... return success immediately ... */
        BPF_STMT(BPF_RET | BPF_K, 0xffffffff),

        /* ... otherwise proceed to the next comparison or exit. */
    };

    mrp_debug("adding pid %d to filter\n", pid);

    memcpy(p, bpf, 3*sizeof(struct sock_filter));
}


static int filter_update(pid_t pids[], int len_pids)
{
    int nl_type_offset = offsetof(struct nlmsghdr, nlmsg_type);
    int cn_offset = NLMSG_LENGTH(0);
    int cn_id_offset = cn_offset + offsetof(struct cn_msg, id);
    int cn_idx_offset = cn_id_offset + offsetof(struct cb_id, idx);
    int cn_val_offset = cn_id_offset + offsetof(struct cb_id, val);
    int proc_offset = cn_offset + offsetof(struct cn_msg, data);
    int proc_what_offset = proc_offset + offsetof(struct proc_event, what);

    struct sock_fprog fp;
    struct sock_filter *bpf, *iter;

    struct sock_filter bpf_header[] = {

        /* check that the message has only one part or is an error
         * ( NLMSG_DONE || NLMSG_ERROR) -- return error immediately */

        /* load the message type */
        BPF_STMT(BPF_LD | BPF_H | BPF_ABS, nl_type_offset),

        /* check the error type */
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, htons(NLMSG_ERROR), 0, 1),

        /* return if error */
        BPF_STMT(BPF_RET | BPF_K, 0xffffffff),

        /* check the done type */
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, htons(NLMSG_DONE), 1, 0),

        /* filter the packet if not NLMSG_DONE */
        BPF_STMT(BPF_RET | BPF_K, 0x0),

        /* check that the message is from the proc connector
         * ( CN_IDX_PROC && CN_VAL_PROC ) */

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, cn_idx_offset),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, htonl(CN_IDX_PROC), 1, 0),
        BPF_STMT(BPF_RET | BPF_K, 0x0),

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, cn_val_offset),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, htonl(CN_VAL_PROC), 1, 0),
        BPF_STMT(BPF_RET | BPF_K, 0x0),

        /* check that the message is a PROC_EVENT_EXIT message */

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, proc_what_offset),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, htonl(PROC_EVENT_EXIT), 1, 0),
        BPF_STMT(BPF_RET | BPF_K, 0x0),
    };

    struct sock_filter bpf_footer[] = {
        /* if there was no pid match then filter the packet */
        BPF_STMT (BPF_RET | BPF_K, 0x0),
    };

    int len_bpf_header = sizeof(bpf_header);
    int len_bpf_footer = sizeof(bpf_footer);
    /* three statements */
    int len_bpf_pids = sizeof(struct sock_filter) * len_pids * 3;
    int len = len_bpf_header + len_bpf_pids + len_bpf_footer;
    int i;

    if (nl_sock <= 0) {
        mrp_log_error("invalid netlink socket %d", nl_sock);
        goto error;
    }

    /* build the filter */

    bpf = (struct sock_filter *) mrp_allocz(len);

    if (!bpf)
        goto error;

    iter = bpf;

    memcpy(iter, bpf_header, len_bpf_header);

    iter = iter + (len_bpf_header / sizeof(struct sock_filter));

    /* check that the PID is one that we are following */
    for (i = 0; i < len_pids; i++, iter=iter+3) {
        filter_add_pid(iter, pids[i], proc_offset);
    }

    memcpy(iter, bpf_footer, len_bpf_footer);

    memset(&fp, 0, sizeof(struct sock_fprog));
    fp.filter = bpf;
    fp.len = len / sizeof(struct sock_filter);

    if (setsockopt(nl_sock, SOL_SOCKET, SO_ATTACH_FILTER, &fp,
            sizeof(struct sock_fprog)) < 0)
        mrp_log_error("setting socket filter failed: %s", strerror(errno));

    mrp_free(bpf);

error:
    return -1;
}


struct key_data_s {
    int index;
    pid_t *pids;
};


static int gather_pids_cb(void *key, void *object, void *user_data)
{
    struct key_data_s *kd = (struct key_data_s *) user_data;
    nl_pid_watch_t *w = (nl_pid_watch_t *) object;

    MRP_UNUSED(key);

    kd->pids[kd->index] = w->pid;
    kd->index++;

    return MRP_HTBL_ITER_MORE;
}


static int pid_filter_update()
{
    pid_t pids[nl_n_pid_watches];

    struct key_data_s kd;

    kd.index = 0;
    kd.pids = pids;

    mrp_htbl_foreach(nl_watches, gather_pids_cb, &kd);

    return filter_update(pids, kd.index);
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


mrp_process_state_t mrp_pid_query_state(pid_t pid)
{
    char path[64];
    struct stat s;
    mrp_process_state_t state = MRP_PROCESS_STATE_UNKNOWN;
    int ret;

    ret = snprintf(path, sizeof(path), "/proc/%u", (unsigned int) pid);

    if (ret < 0 || ret >= (int) sizeof(path))
        goto end;

    ret = stat(path, &s);

    if (ret < 0 && (errno == ENOENT || errno == ENOTDIR)) {
        state = MRP_PROCESS_STATE_NOT_READY;
    }
    else if (ret == 0 && S_ISDIR(s.st_mode)) {
        state = MRP_PROCESS_STATE_READY;
    }

end:
    return state;
}


int mrp_process_set_watch(const char *id, mrp_mainloop_t *ml,
        mrp_process_watch_handler_t cb, void *userdata)
{
    i_watch_t *w = NULL;

    if (initialize_process(ml) < 0)
        goto error;

    w = (i_watch_t *) mrp_allocz(sizeof(i_watch_t));

    if (!w)
        goto error;

    w->inotify_cb = i_wd;
    w->userdata = userdata;
    w->process_cb = cb;

    w->path = mrp_strdup(id);
    if (!w->path)
        goto error;

    w->filename = path_from_id(id);
    if (!w->filename)
        goto error;

    if (mrp_htbl_insert(i_watches, w->filename, w) < 0)
        goto error;

    i_n_process_watches++;

    return 0;

error:
    if (w) {
        mrp_free(w->path);
        mrp_free(w->filename);
        mrp_free(w);
    }

    return -1;
}


mrp_pid_watch_t *mrp_pid_set_watch(pid_t pid, mrp_mainloop_t *ml,
        mrp_pid_watch_handler_t cb, void *userdata)
{
    nl_pid_watch_t *nl_w = NULL;
    nl_pid_client_t *client = NULL;
    char pid_s[16];
    bool already_inserted = TRUE;
    int ret;

    if (initialize_pid(ml) < 0)
        goto error;

    ret = snprintf(pid_s, sizeof(pid_s), "%u", (unsigned int) pid);

    if (ret < 0 || ret >= (int) sizeof(pid_s))
        goto error;

    nl_w = (nl_pid_watch_t *) mrp_htbl_lookup(nl_watches, pid_s);

    if (!nl_w) {

        nl_w = (nl_pid_watch_t *) mrp_allocz(sizeof(nl_pid_watch_t));

        if (!nl_w)
            goto error;

        mrp_list_init(&nl_w->clients);
        nl_w->pid = pid;
        memcpy(nl_w->pid_s, pid_s, sizeof(nl_w->pid_s));

        already_inserted = FALSE;
    }

    client = (nl_pid_client_t *) mrp_allocz(sizeof(nl_pid_client_t));

    if (!client) {
        if (!already_inserted)
            mrp_free(nl_w);
        goto error;
    }

    client->cb = cb;
    client->user_data = userdata;
    client->w = (mrp_pid_watch_t *) mrp_allocz(sizeof(mrp_pid_watch_t));

    if (!client->w) {
        mrp_free(nl_w);
        goto error;
    }

    client->w->pid = pid;

    mrp_list_init(&client->hook);
    mrp_list_append(&nl_w->clients, &client->hook);
    nl_w->n_clients++;

    if (!already_inserted) {
        if (mrp_htbl_insert(nl_watches, nl_w->pid_s, nl_w) < 0) {
            mrp_list_delete(&client->hook);
            mrp_free(nl_w);
            goto error;
        }

        nl_n_pid_watches++;
    }

    pid_filter_update();

    if (!subscribed)
        subscribe_proc_events();

    /* check that the pid is still there -- return error if not */

    if (mrp_pid_query_state(pid) != MRP_PROCESS_STATE_READY)
        goto error_process;

    return client->w;

error_process:
    mrp_pid_remove_watch(client->w);
    client = NULL;

error:
    if (client) {
        mrp_free(client);
        mrp_free(client->w);
    }

    return NULL;
}


static void update_map()
{
    if (i_n_process_watches <= 0) {
        if (i_fd > 0 && dir_fd > 0) {
           inotify_rm_watch(i_fd, dir_fd);
           dir_fd = -1;
        }
        i_n_process_watches = 0;
    }

    if ((i_n_process_watches) == 0) {
        mrp_htbl_destroy(i_watches, TRUE);
        i_watches = NULL;
    }
}


int mrp_process_remove_watch(const char *id)
{
    i_watch_t *w;
    char *filename;

    if (!i_watches)
        return -1;

    filename = path_from_id(id);

    w = (i_watch_t *) mrp_htbl_lookup(i_watches, (void *)filename);

    if (!w) {
        mrp_free(filename);
        return -1;
    }

    mrp_htbl_remove(i_watches, (void *) filename, TRUE);
    i_n_process_watches--;

    update_map();

    mrp_free(filename);
    return 0;
}


int mrp_pid_remove_watch(mrp_pid_watch_t *w)
{
    nl_pid_watch_t *nl_w = NULL;
    nl_pid_client_t *client = NULL;
    char pid_s[16];
    mrp_list_hook_t *p, *n;
    bool found = FALSE;
    int ret;

    if (!w)
        goto error;

    ret = snprintf(pid_s, sizeof(pid_s), "%u", (unsigned int) w->pid);

    if (ret < 0 || ret >= (int) sizeof(pid_s))
        goto error;

    nl_w = (nl_pid_watch_t *) mrp_htbl_lookup(nl_watches, pid_s);

    if (!nl_w) {
        mrp_log_error("no corresponding pid watch found");
        goto error;
    }

    mrp_list_foreach(&nl_w->clients, p, n) {
        client = mrp_list_entry(p, typeof(*client), hook);
        if (client->w == w) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        mrp_log_error("not registered to watch the pid");
        goto error;
    }

    mrp_list_delete(&client->hook);
    mrp_free(client);
    nl_w->n_clients--;

    if (nl_w->n_clients == 0) {
        /* no-one is interested in this pid anymore */
        mrp_htbl_remove(nl_watches, pid_s, TRUE);
        nl_n_pid_watches--;

        pid_filter_update();

        if (nl_n_pid_watches == 0) {
            /* no-one is following pids anymore */
            if (subscribed)
                unsubscribe_proc_events();
        }
    }

    mrp_free(w);
    return 0;

error:
    mrp_free(w);
    return -1;
}
