#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <murphy/common.h>
#include <murphy/core.h>
#include <murphy/plugins/signalling/signalling-protocol.h>

typedef struct {
    mrp_transport_t *t;
    mrp_mainloop_t *ml;
} client_t;


static void dump_decision(client_t *c, ep_decision_t *msg)
{
    uint i;

    MRP_UNUSED(c);

    printf("Message contents:\n");
    for (i = 0; i < msg->n_rows; i++) {
        printf("row %d: '%s'\n", i+1, msg->rows[i]);
    }
    printf("%s required.\n\n", msg->reply_required ? "Reply" : "No reply");

}

static int send_registration(client_t *c)
{
    char *name = "test ep";
    char *domains[] = { "domain1", "domain2" };
    ep_register_t msg;
    int ret;

    msg.ep_name = name;
    msg.domains = domains;
    msg.n_domains = 2;

    ret = mrp_transport_senddata(c->t, &msg, TAG_REGISTER);

    if (!ret) {
        printf("failed to send register message\n");
    }

    return ret;
}


static int send_reply(client_t *c, ep_decision_t *msg, uint32_t success)
{
    ep_ack_t reply;
    int ret;

    reply.id = msg->id;
    reply.success = success;

    ret = mrp_transport_senddata(c->t, &reply, TAG_ACK);

    if (!ret) {
        printf("failed to send reply\n");
    }

    return ret;
}


static void handle_decision(client_t *c, ep_decision_t *msg)
{
    printf("Handle decision\n");

    dump_decision(c, msg);

    if (msg->reply_required)
        send_reply(c, msg, EP_ACK);

    return;
}


static void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    (void) t;
    (void) error;
    (void) user_data;
    printf("Received closed event\n");
}


static void recvfrom_evt(mrp_transport_t *t, void *data, uint16_t tag,
             mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    client_t *c = user_data;

    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);
    MRP_UNUSED(user_data);
    MRP_UNUSED(t);

    printf("Received message (0x%02x)\n", tag);

    switch (tag) {
        case TAG_POLICY_DECISION:
            handle_decision(c, data);
            break;
        case TAG_ERROR:
            printf("Server sends an error message!\n");
            break;
        default:
            /* no other messages supported ATM */
            break;
    }

}


static void recv_evt(mrp_transport_t *t, void *data, uint16_t tag, void *user_data)
{
    recvfrom_evt(t, data, tag, NULL, 0, user_data);
}


int main()
{
    socklen_t alen;
    mrp_sockaddr_t addr;
    int ret, flags;

    static client_t client;
    static mrp_transport_evt_t evt; /* static members are initialized to zero */

    evt.closed = closed_evt;
    evt.recvdatafrom = recvfrom_evt;
    evt.recvdata = recv_evt;

    if (!mrp_msg_register_type(&ep_register_descr) ||
        !mrp_msg_register_type(&ep_decision_descr) ||
        !mrp_msg_register_type(&ep_ack_descr)) {
        printf("Registering data type failed!\n");
        exit(1);
    }

    client.ml = mrp_mainloop_create();

    flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_MODE_CUSTOM;

    client.t = mrp_transport_create(client.ml, "unxs", &evt, &client, flags);
    if (client.t == NULL) {
        printf("Error creating a new transport!\n");
        exit(1);
    }

    alen = mrp_transport_resolve(NULL, "unxs:/tmp/murphy/signalling", &addr, sizeof(addr), NULL);
    if (alen <= 0) {
        printf("Error resolving address! Maybe the host is not running?\n");
        exit(1);
    }


    ret = mrp_transport_connect(client.t, &addr, alen);
    if (ret == 0) {
        printf("Connect failed!\n");
        exit(1);
    }

    send_registration(&client);

    mrp_mainloop_run(client.ml);

    return 0;
}
