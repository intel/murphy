/*
 * debugging
 */

var DOMCTL_COMM   = 0;                      /* message/communication */
var DOMCTL_NOTIFY = 1;                      /* data notification */
var DOMCTL_MISC   = 2;                      /* other debug messages */

var domctl_debug_names = [ 'COMM', 'NOTIFY', 'MISC'];
var domctl_debug_mask  = 0x0;


function domctl_debug_index_of(name) {
    for (var idx in domctl_debug_names) {
        if (domctl_debug_names[idx] == name)
            return idx;
    }

    return -1;
}


function domctl_debug_mask_of(name) {
    var idx = domctl_debug_index_of(name);

    if (idx >= 0)
        return (1 << domctl_debug_index_of(name));
    else
        return 0;
}


function domctl_debug_set(flags, enable) {
    var mask = 0;
    var flag;

    if (typeof flags == typeof "" || typeof flags == typeof 1)
        flags = [ flags ];

    for (var idx in flags) {
        flag = flags[idx];

        if (typeof flag == typeof "")
            mask |= domctl_debug_mask_of(flag);
        else
            mask |= (1 << flag);
    }

    if (enable)
        domctl_debug_mask |= mask;
    else
        domctl_debug_mask &= ~mask;
}


function domctl_debug_enable (flags) {
    domctl_debug_set(flags, true);
}


function domctl_debug_disable(flags) {
    domctl_debug_set(flags, false);
}


function domctl_debug() {
    var flag, msg, i;

    if (arguments.length >= 2) {
        flag = arguments[0];
        mask = (1 << flag);

        if (!(mask & domctl_debug_mask))
            return;

        flag = domctl_debug_names[flag];

        for (i = 1, msg = ""; i < arguments.length; i++) {
            msg += arguments[i];
        }

        console.log("D: [" + flag + "] " + msg);
    }
    else {
        if (arguments.length == 1)
            console.log("D: [ALL] " + arguments[0]);
    }
}


/*
 * custom errors
 */

function DomainControllerError(message) {
    this.name    = "Domain Control Error";
    this.message = message;
}


/*
 * pending requests
 */

/** Contruct a new pending request for the given request and controller. */
function DomainControllerPendingRequest (controller, req) {
    this.controller = controller;
    this.req        = req;
    this.reqno      = req.seq;
}


/** Deliver pending request reply notification. */
DomainControllerPendingRequest.prototype.notify = function (message) {
    var event;

    switch (message.type) {
    case 'ack':
        if (this.onsuccess) {
            event = { type: 'ack', seq: message.seq };
            this.onsuccess(event);
        }
        break;

    case 'nak':
        if (this.onerror) {
            event = { type: 'nak', seq: message.seq };
            event.error  = message.error  ? message.error  : -1;
            event.errmsg = message.errmsg ? message.errmsg : "<unknown error>";
            this.onerror(message);
        }
        break;

    default:
        break;
    }
}


/*
 * DomainController
 */

/** Reset controller state to defaults. */
DomainController.prototype.reset = function () {
    this.connected = false;
    this.server    = null;
    this.sck       = null;
    this.reqno     = 1;
    this.reqq      = [];
    this.sets      = [];

    this.ondisconnect = null;
    this.onfailed     = null;
    this.onevent      = null;
}


/** Ensure we have a connection, throw an error otherwise. */
DomainController.prototype.check_connection = function () {
    if (!this.connected)
        throw new DomainControllerError("not connected");
}


/** Event handler for connection establishment. */
DomainController.prototype.sckopen = function () {
    var ctl = this.controller;

    domctl_debug(DOMCTL_COMM, "connected to server " + ctl.server);

    ctl.connected = true;
    ctl.register();
}


/** Event handler for socket disconnection. */
DomainController.prototype.sckclose = function () {
    var ctl = this.controller;

    domctl_debug(DOMCTL_COMM, "disconnected from server");

    ctl.connected = false;

    if (ctl.ondisconnect)                /* notify listener if any */
        ctl.ondisconnect();

    ctl.reset();
}


/** Event handler for connection error. */
DomainController.prototype.sckerror = function () {
    var ctl = this.controller;

    domctl_debug(DOMCTL_COMM, "connection error");

    ctl.connected = false;

    if (ctl.onerror) {
        ctl.ondisconnect = null;
        ctl.onerror();
    }
}



/** Event handler for receiving messages. */
DomainController.prototype.sckmessage = function (message) {
    var ctl = this.controller;
    var msg = JSON.parse(message.data);
    var seq = msg.seq;
    var pending;

    domctl_debug(DOMCTL_COMM, "received message: " + message.data);

    switch (msg.type) {
    case 'notify':
        ctl.notify(msg);
        break;

    case 'ack':
    case 'nak':
        pending = ctl.deq(msg.seq);

        if (pending) {
            pending.notify(msg);
        }
        break;
    }
}


/** Deliver domain controller event notification. */
DomainController.prototype.notify = function (message) {
    var idx, t, w;
    var event;

    if (this.onevent) {
        event = {
            type: 'notify',
            tables: {}
        };

        for (idx in message.tables) {
            t = message.tables[idx];
            w = this.watches[idx];

            if (w) {
                event.tables[w.table] = {
                    id:   w.id,
                    rows: t.rows,
                };
            }
        }

        this.onevent(event);
    }
}


/** Enqueue an outgoing request to the server. */
DomainController.prototype.enq = function (req) {
    var seq, pending;

    seq     = req.seq = this.reqno++;
    pending = new DomainControllerPendingRequest(this, req);

    this.reqq[seq] = pending;

    return pending;
}


/** Dequeue a pending request for the given sequence number. */
DomainController.prototype.deq = function (seq) {
    var pending;

    pending = this.reqq[seq];

    if (pending)
        delete this.reqq[seq];

    return pending;
}


/** Send a request to the server returning a pending object if appropriate. */
DomainController.prototype.send_request = function (req) {
    var pending;

    if (req.type != 'register')
        pending = this.enq(req);
    else {
        req.seq = 0;
        pending = null;
    }

    domctl_debug(DOMCTL_COMM, "sending message: " + JSON.stringify(req));

    this.sck.send(JSON.stringify(req));

    return pending;
}


/** Send register message to the server. */
DomainController.prototype.register = function () {
    var f, ntable, nwatch, req;

    ntable = 0;
    for (f in this.tables) {
        this.tables[f].id = ntable++;
    }

    nwatch = 0;
    for (f in this.watches) {
        this.watches[f].id = nwatch++;
    }

    req = {
        type:    'register',
        name:    this.name,
        tables:  this.tables,
        ntable:  ntable,
        watches: this.watches,
        nwatch:  nwatch,
    };

    this.send_request(req);
}


/** Create a new domain controller object. */
function DomainController(name, tables, watches) {
    this.reset();

    this.name    = name;
    this.tables  = tables;
    this.watches = watches;
}


/** Determine a WebSocket URI based on an HTTP URI. */
DomainController.prototype.socketUri = function (http_uri) {
    var proto, colon, rest;

    colon = http_uri.indexOf(':');           /* get first colon */
    proto = http_uri.substring(0, colon);    /* get protocol */
    rest  = http_uri.substring(colon + 3);   /* get URI sans protocol:// */
    addr  = rest.split("/")[0];              /* strip URI path from address */

    switch (proto) {
    case "http":  return "ws://"  + addr;
    case "https": return "wss://" + addr;
    default:      return null;
    }
}


/** Initiate connection to the given server. */
DomainController.prototype.connect = function (server) {
    if (this.connected)
        throw new DomainControllerError("already connected to " + this.server);
    else {
        domctl_debug(DOMCTL_COMM, "trying to connect to " + server);
        this.server = server;

        if (typeof MozWebSocket != "undefined")
            this.sck = new MozWebSocket(this.server, "murphy");
        else
            this.sck = new WebSocket(this.server, "murphy");

        this.sck.controller = this;
        this.sck.onopen     = this.sckopen;
        this.sck.onclose    = this.sckclose;
        this.sck.onerror    = this.sckerror;
        this.sck.onmessage  = this.sckmessage;
    }
}


/** Disconnect from the server. */
DomainController.prototype.disconnect = function () {
    this.check_connection();

    domctl_debug(DOMCTL_COMM, "disconnecting from " + this.server);

    this.sck.close();
    delete this.sck;
}


/** Set data on server. */
DomainController.prototype.set = function (table_data) {
    var idx, id, name;
    var table, ntbl, ntot, ncol, nrow;
    var req;

    req = { type: 'set', seq: 0, nchange: 0, ntotal: 0, tables: [] };

    ntbl = ntot = 0;
    for (idx in table_data) {
        ntbl++;
        data  = table_data[idx];
        table = data.table;
        rows  = data.rows;

        ncol  = rows[0].length;
        nrow  = rows.length;
        ntot += ncol * nrow;

        id = -1;
        for (tbl in this.tables) {
            if (this.tables[tbl].table == table)
                id = this.tables[tbl].id;
        }
        if (id < 0)
            throw new DomainControllerError("unknown table " + table);

        name = this.tables[id].table;

        req.tables[idx] = { id: id, nrow: nrow, ncol: ncol, rows: rows };
    }

    req.nchange = ntbl;
    req.ntotal  = ntot;

    return this.send_request(req);
}
