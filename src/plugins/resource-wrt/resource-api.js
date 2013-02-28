

/*
 * debugging
 */

var WRT_MGR = 0;                         /* resource manager debugging */
var WRT_MSG = 1;                         /* message debugging */
var WRT_SET = 2;                         /* resource set debugging */

var wrt_debug_names = [ 'MGR', 'MSG', 'SET'];
var wrt_debug_mask  = 0x0;


function wrt_debug_index_of(name) {
    for (var idx in wrt_debug_names) {
        if (wrt_debug_names[idx] == name)
            return idx;
    }

    return -1;
}


function wrt_debug_mask_of(name) {
    var idx = wrt_debug_index_of(name);

    if (idx >= 0)
        return (1 << wrt_debug_index_of(name));
    else
        return 0;
}


function wrt_debug_set(flags, enable) {
    var mask = 0;
    var flag;

    if (typeof flags == typeof "" || typeof flags == typeof 1)
        flags = [ flags ];

    for (var idx in flags) {
        flag = flags[idx];

        if (typeof flag == typeof "")
            mask |= wrt_debug_mask_of(flag);
        else
            mask |= (1 << flag);
    }

    if (enable)
        wrt_debug_mask |= mask;
    else
        wrt_debug_mask &= ~mask;
}


function wrt_debug_enable (flags) {
    wrt_debug_set(flags, true);
}


function wrt_debug_disable(flags) {
    wrt_debug_set(flags, false);
}


function wrt_debug() {
    var flag, msg, i;

    if (arguments.length >= 2) {
        flag = arguments[0];
        mask = (1 << flag);

        if (!(mask & wrt_debug_mask))
            return;

        flag = wrt_debug_names[flag];

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
 * our custom error type
 */

function WrtResourceError(message) {
    this.name    = "Resource Error";
    this.message = message;
}


/*
 * resource manager
 */

WrtResourceManager.prototype.reset = function () {
    this.connected = false;              /* no connection */
    this.server    = null;               /* no server */
    this.sck       = null;               /* no socket */
    this.reqno     = 1;                  /* next request sequence number */
    this.reqq      = [];                 /* empty request queue */
    this.sets      = [];                 /* no resource sets */

    this.onconnect    = null;            /* clear connection callback */
    this.ondisconnect = null;            /* clear disconnect callback */
    this.onfailed     = null;
}


/** Ensure we have a connection, throw an error if we don't. */
WrtResourceManager.prototype.check_connection = function () {
    if (!this.connected)
        throw new WrtResourceError("not connected");
}


/** Event handler for server socket connection. */
WrtResourceManager.prototype.sckopen = function () {
    var mgr = this.manager;

    wrt_debug(WRT_MGR, "connected to server " + mgr.server);
    wrt_debug(WRT_MGR, "mgr.sck = " + mgr.sck);

    mgr.connected = true;

    if (mgr.onconnect)                   /* notify listener if any */
        mgr.onconnect();
}


/** Event handler for server socket disconnection. */
WrtResourceManager.prototype.sckclose = function () {
    var mgr = this.manager;

    wrt_debug(WRT_MGR, "disconnected from server");

    mgr.connected = false;

    if (mgr.ondisconnect)                /* notify listener if any */
        mgr.ondisconnect();

    mgr.reset();
}


/** Event handler for server socket connection error. */
WrtResourceManager.prototype.sckerror = function () {
    var mgr = this.manager;

    wrt_debug(WRT_MGR, "failed to connect to server " + mgr.server);

    if (mgr.onfailed) {                  /* notify listener if any */
        mgr.ondisconnect = null;         /* only call error handler */
        mgr.onfailed();
    }
}


/** Event handler for incoming message. */
WrtResourceManager.prototype.sckmessage = function (message) {
    var mgr = this.manager;
    var msg = JSON.parse(message.data);
    var seq = msg.seq;
    var pending, rset;

    wrt_debug(WRT_MSG, "received ", msg.type, " message (#", seq, ")");

    if (msg.type == 'event') {
        rset = mgr.sets[msg.id];

        if (rset) {
            delete mgr.reqq[seq];
            rset.notify(msg);
        }
    }
    else {
        pending = mgr.reqq[seq];

        if (pending) {
            delete mgr.reqq[seq];

            pending.notify(msg);
        }
    }
}


/** Resource set constructor. */
function WrtResourceSet (mgr, reqno) {
    this.manager = mgr;
    this.reqno   = reqno;
}


/** Deliver resource set event notification. */
WrtResourceSet.prototype.notify = function (msg) {
    var type = msg.type;

    if (type == 'event') {
        wrt_debug(WRT_MSG, "resource set notification...");

        if (!this.resources)
            this.resources = msg.resources;

        this.state  = msg.state;
        this.grant  = msg.grant;
        this.advice = msg.advice;

        if (this.onstatechanged)
            this.onstatechanged(this.grant);
    }
    else if (type == 'create') {
        var status = msg.status;
        var mgr    = this.manager;

        if (status == 0) {
            this.id = msg.id;
            mgr.sets[this.id] = this;

            if (this.onsuccess)
                this.onsuccess();
        }
        else if (this.onerror) {
            var error = {
                error:   msg.error   ? msg.error   : 1,
                message: msg.message ? msg.message : "<server-side error>"
            };

            this.onerror(error);
        }
    }
}


/** Map resources to names. */
WrtResourceSet.prototype.ensure_resource_map = function () {
    var r;

    if (this.resource_by_name)
        return;

    if (!this.resources)
        throw new WrtError("resources/masks not known yet");

    this.resource_by_name = {};

    for (var i in this.resources) {
        r = this.resources[i];
        this.resource_by_name[r.name] = {
            mask: r.mask,
            attributes: r.attributes
        }
    }
}


/** Pending request constructor. */
function WrtPendingRequest (mgr, reqno) {
    this.manager = mgr;
    this.reqno   = reqno;
}


WrtPendingRequest.prototype.map_resources = function (e) {
    var i, r, m;

    m = {};
    for (i in e) {
        r = e[i];

        if (r.attributes)
            m[r.name] = r.attributes;
        else
            m[r.name] = {};
    }

    return m;
}


/** Deliver pending request reply notification.*/
WrtPendingRequest.prototype.notify = function (msg) {
    var evtmap = {
        'query-classes'  : { field: 'classes' },
        'query-zones'    : { field: 'zones'   },
        'query-resources': { field: 'resources', map: this.map_resources }
    };
    var status = msg.status;
    var event, m;

    if (status == 0) {
        if (this.onsuccess && (m = evtmap[msg.type])) {
            event = m.map ? m.map(msg[m.field]) : msg[m.field];
            this.onsuccess(event);
        }
    }
    else {
        if (this.onerror) {
            event = {
                error:   msg.error   ? msg.error   : 666,
                message: msg.message ? msg.message : "<server-side error>"
            };
            this.onerror(event);
        }
    }
}


/** Send a request to the server, return a pending request object for it. */
WrtResourceManager.prototype.send_request = function (req) {
    var pending, msg, seq;

    seq = this.reqno++;

    wrt_debug(WRT_MSG, "sending ", req.type, " request (#", seq, ")");

    if (req.type == 'create')
        pending = new WrtResourceSet(this, seq);
    else
        pending = new WrtPendingRequest(this, seq);

    req.seq        = seq;
    pending.req    = req;
    this.reqq[seq] = pending;

    this.sck.send(JSON.stringify(pending.req));

    return pending;
}


/*
 * Public Resource Manager API
 */

/** Resource Manager constructor. */
function WrtResourceManager() {
    this.reset();
}


/** Initiate connection to the given server. */
WrtResourceManager.prototype.connect = function (server) {
    if (this.connected)
        throw new WrtResourceError("already connected");
    else {
        wrt_debug(WRT_MGR, "trying to connect to " + server);

        this.server = server

        if (typeof MozWebSocket != "undefined")
            this.sck = new MozWebSocket(this.server, "murphy");
        else
            this.sck = new WebSocket(this.server, "murphy");

        this.sck.manager   = this;
        this.sck.onopen    = this.sckopen;
        this.sck.onclose   = this.sckclose;
        this.sck.onerror   = this.sckerror;
        this.sck.onmessage = this.sckmessage;
    }
}


/** Disconnect from the server. */
WrtResourceManager.prototype.disconnect = function () {
    this.check_connection();

    wrt_debug(WRT_MGR, "disconnecting from " + this.server);

    this.sck.close();
    delete this.sck;
}


/** Initiate an application class name query. */
WrtResourceManager.prototype.queryApplicationClassNames = function () {
    this.check_connection();

    wrt_debug(WRT_MGR, "initiating application class query");

    return this.send_request({ type: 'query-classes'});
}


/** Initiate zone name query. */
WrtResourceManager.prototype.queryZoneNames = function () {
    this.check_connection();

    wrt_debug(WRT_MGR, "initiating zone query");

    return this.send_request({ type: 'query-zones' });
}


/** Initiate resource definition query. */
WrtResourceManager.prototype.queryResourceDefinitions = function () {
    this.check_connection();

    wrt_debug(WRT_MGR, "initiating resource query");

    return this.send_request({ type: 'query-resources'});
}


/** Create a new resource set. */
WrtResourceManager.prototype.createResourceSet = function (resources, options) {
    var appClass, zone, priority, flags;
    var req;

    this.check_connection();

    wrt_debug(WRT_MGR, "creating new resource set");

    appClass = options.class    ? options.class    : "player";
    zone     = options.zone     ? options.zone     : "driver";
    priority = options.priority ? options.priority : 0;

    return this.send_request({ type: 'create',
                               class: appClass, zone: zone, priority: priority,
                               resources: resources });
}

/** Determine a WebSocket URI based on an HTTP URI. */
WrtResourceManager.prototype.socketUri = function (http_uri) {
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

/** Acquire the resource set. */
WrtResourceSet.prototype.acquire = function () {
    this.manager.send_request({ type: 'acquire', id: this.id });
}


/** Release the resource set. */
WrtResourceSet.prototype.release = function () {
    this.manager.send_request({ type: 'release', id: this.id });
}


/** Get the mask of granted resources. */
WrtResourceSet.prototype.getGrantedMask = function () {
    return this.grant;
}


/** Get the mask of allocable resources. */
WrtResourceSet.prototype.getAllocableMask = function () {
    return this.advice;
}


/** Get resource mask by resource name. */
WrtResourceSet.prototype.getMaskByResourceName = function (name) {
    var r;

    this.ensure_resource_map();

    if ((r = this.resource_by_name[name]))
        return r.mask;
    else
        return 0;
}


/** Get resource attributes by name. */
WrtResourceSet.prototype.getAttributesByResourceName = function (name) {
    var r;

    this.ensure_resource_map();

    if ((r = this.resource_by_name[name]))
        return r.attributes;
    else
        return {};
}


/** Get the names of all resources in the set. */
WrtResourceSet.prototype.getResourceNames = function () {
    var names, i;

    names = [];
    for (var i in this.resources) {
        names.push(this.resources[i].name);
    }

    return names;
}


/** Get the names of granted resources in the set. */
WrtResourceSet.prototype.getGrantedResourceNames = function () {
    var names, n;

    this.ensure_resource_map();

    names = [];

    for (var n in this.resource_by_name) {
        r = this.resource_by_name[n];
        if (this.grant & r.mask)
            names.push(n);
    }

    return names;
}


/** Get the names of granted resources in the set. */
WrtResourceSet.prototype.getAllocableResourceNames = function () {
    var names, n;

    this.ensure_resource_map();

    names = [];

    for (var n in this.resource_by_name) {
        r = this.resource_by_name[n];
        if (this.advice & r.mask)
            names.push(n);
    }

    return names;
}


/** Check if the named resource has been granted. */
WrtResourceSet.prototype.isGranted = function (name) {
    this.ensure_resource_map();

    r = this.resource_by_name[name];

    if (this.grant & r.mask)
        return true;
    else
        return false;
}


/** Check if the named resource can be allocated. */
WrtResourceSet.prototype.isAllocable = function (name) {
    this.ensure_resource_map();

    r = this.resource_by_name[name];

    if (this.advice & r.mask)
        return true;
    else
        return false;
}


/** Check if the set has been released. */
WrtResourceSet.prototype.isReleased = function () {
    return (this.state == 'release');
}


/** Check if the set has been pre-empted. */
WrtResourceSet.prototype.isPreempted = function () {
    return (this.state == 'acquire' && this.grant == 0);
}
