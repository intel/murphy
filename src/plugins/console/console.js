/*
 * An Ode to My Suckage in Javascript...
 *
 * It'd be nice if someone wrote a relatively simple readline-like + output
 * javascript package (ie. not a full VT100 terminal emulator like termlib).
 */


/*
 * custom console error type
 */

function MrpConsoleError(message) {
    this.name    = "Murphy Console Error";
    this.message = message;
}


/** Create a Murphy console, put it after next_elem_id. */
function MrpConsole(next_elem_id, user_commands) {
    var sck, input, output, div, next_elem;

    this.reset();
    this.commands = user_commands;

    /* create output text area */
    output = document.createElement("textarea");
    output.console = this;
    this.output    = output;

    output.setAttribute("class"     , "console_output");
    output.setAttribute("cols"      , 80);
    output.setAttribute("rows"      , 25);
    output.setAttribute("readonly"  , true);
    output.setAttribute("disabled"  , true);
    output.setAttribute("spellcheck", false);
    output.nline   = 0;
    output.onfocus = function () { return false; }

    /* create input text area, hook up input handler */
    input = document.createElement("textarea");
    input.console = this;
    this.input    = input;

    input.setAttribute("class"     , "console_input");
    input.setAttribute("cols"      , 81);
    input.setAttribute("rows"      ,  1);
    input.setAttribute("spellcheck", false);
    input.setAttribute("autofocus" , "autofocus");

    input.onkeyup    = this.onkeyup;
    input.onkeypress = this.onkeypress;

    next_elem = document.getElementById(next_elem_id);

    if (!next_elem)
        throw new MrpConsoleError("element " + next_elem_id + " not found");

    div = document.createElement("div");
    div.appendChild(output);
    div.appendChild(input);

    /* insert console div to document */
    document.body.insertBefore(div, next_elem);

    this.setInput("");
}


/** Reset/initialize internal state to the disconnected defaults. */
MrpConsole.prototype.reset = function () {
    this.connected = false;
    this.server    = null;
    this.sck       = null;

    this.history    = new Array ();
    this.histidx    = 0;
    if (!this.input)
        this.prompt     = "disconnected";
    else
        this.setPrompt("disconnected");
}


/** Resize the console. */
MrpConsole.prototype.resize = function (width, height) {
    if (width && width > 0) {
        this.output.cols = width;
        this.input.cols  = width;
    }
    if (height && height > 0)
        this.output.rows = height;
}


/** Get the focus to the console. */
MrpConsole.prototype.focus = function () {
    if (this.input)
        this.input.focus();
}


/** Write output to the console, replacing its current contents. */
MrpConsole.prototype.write = function (text, noscroll) {
    var out = this.output;

    out.value = text;
    out.nline = text.split("\n").length;

    if (!noscroll)
        this.scrollBottom();
}


/** Append output to the console. */
MrpConsole.prototype.append = function (text, noscroll) {
    var out = this.output;

    out.value += text;
    out.nline += text.split("\n").length;

    if (!noscroll)
        this.scrollBottom();
}


/** Set the content of the input field to 'prompt> text'. */
MrpConsole.prototype.setInput = function (text) {
    this.input.value = this.prompt + "> " + text;
}


/** Get the current input value (without the prompt). */
MrpConsole.prototype.getInput = function () {
    if (this.input)
        return this.input.value.slice(this.prompt.length + 2).split("\n")[0];
    else
        return "";
}


/** Set the input prompt to the given value. */
MrpConsole.prototype.setPrompt = function (prompt) {
    var value = this.getInput();

    if (!this.input)
        this.prompt = prompt;
    else {
        this.prompt = prompt;
        this.input.value = this.prompt + "> " + value;
    }
}


/** Scroll the output window up or down the given amount of lines. */
MrpConsole.prototype.scroll = function (amount) {
    var out = this.output;
    var pxl = (out.nline ? (out.scrollHeight / out.nline) : 0);
    var top = out.scrollTop + (amount * pxl);

    if (top < 0)
        top = 0;
    if (top > out.scrollHeight)
        top = out.scrollHeight;

    out.scrollTop = top;
}


/** Scroll the output window up or down by a 'page'. */
MrpConsole.prototype.scrollPage = function (dir) {
    var out   = this.output;
    var nline = 2 * 25 / 3;

    if (dir < 0)
        dir = -1;
    else
        dir = +1;

    this.scroll(dir * nline);
}


/** Scroll to the bottom. */
MrpConsole.prototype.scrollBottom = function () {
    this.output.scrollTop = this.output.scrollHeight;
}


/** Add a new entry to the history. */
MrpConsole.prototype.historyAppend = function (entry) {
    if (entry.length > 0) {
        this.history.push(entry);
        this.histidx = this.history.length;

        this.setInput("");
    }
}


/** Go to the previous history entry. */
MrpConsole.prototype.historyShow = function (dir) {
    var idx = this.histidx + dir;

    if (0 <= idx && idx < this.history.length) {
        if (this.histidx == this.history.length &&
            this.input.value.length > 0) {
            /* Hmm... autoinsert to history, not the Right Thing To Do... */
            this.historyAppend(this.getInput());
        }

        this.histidx = idx;
        this.setInput(this.history[this.histidx]);
    }
    else if (idx >= this.history.length) {
        this.histidx = this.history.length;
        this.setInput("");
    }
}


/** Make sure the input position never enters the prompt. */
MrpConsole.prototype.checkInputPosition = function () {
    var pos = this.input.selectionStart;

    if (pos <= this.prompt.length + 2) {
        this.input.selectionStart = this.prompt.length + 2;
        this.input.selectionEnd   = this.prompt.length + 2;
        return false;
    }
    else
        return true;
}


/** Key up handler. */
MrpConsole.prototype.onkeyup = function (e) {
    var c = this.console;
    var l;

    /*console.log("got key " + e.which);*/

    switch (e.keyCode) {
    case e.DOM_VK_RETURN:
        if (c.input.value.length > c.prompt.length + 2) {
            l = c.getInput();
            c.historyAppend(l);
            c.processCmd(l);
            c.setInput("");
        }
        break;
    case e.DOM_VK_PAGE_UP:
        if (e.shiftKey)
            c.scrollPage(-1);
        break;
    case e.DOM_VK_PAGE_DOWN:
        if (e.shiftKey)
            c.scrollPage(+1);
        break;

    case e.DOM_VK_UP:
        if (!e.shiftKey)
            c.historyShow(-1);
        else
            c.scroll(-1);
        break;
    case e.DOM_VK_DOWN:
        if (!e.shiftKey)
            c.historyShow(+1);
        else
            c.scroll(+1);
        break;
    case e.DOM_VK_LEFT:
    case e.DOM_VK_BACK_SPACE:
        if (!c.checkInputPosition())
            return false;
        break;
    }

    return true;
}


/** Key-press handler. */
MrpConsole.prototype.onkeypress = function (e) {
    var c = this.console;
    var rows;

    switch (e.which) {
    case e.DOM_VK_LEFT:
    case e.DOM_VK_BACK_SPACE:
        if (!c.checkInputPosition())
            return false;
    }

    rows = Math.floor(1 + (c.input.value.length / c.input.cols));

    if (c.input.rows < rows)
        c.input.rows = rows;
    else if (c.input.rows > rows)
        c.input.rows = rows;

    return true;
}


/** Connect to the Murphy daemon running at the given address. */
MrpConsole.prototype.connect = function (address) {
    var c = this.console;

    if (this.connected)
        throw new MrpConsoleError("already connected to " + this.address);
    else {
        this.server    = address;
        this.connected = false;

        this.setPrompt("connecting");

        if (typeof MozWebSocket != "undefined")
            sck = new MozWebSocket(this.server, "murphy");
        else
            sck = new WebSocket(this.server, "murphy");

        this.sck      = sck;
        sck.console   = this;
        sck.onopen    = this.sckconnect;
        sck.onclose   = this.sckclosed;
        sck.onerror   = this.sckerror;
        sck.onmessage = this.sckmessage;
    }
}


/** Close the console connection. */
MrpConsole.prototype.disconnect = function () {
    if (this.connected) {
        this.sck.close();
    }
}


/** Connection established event handler. */
MrpConsole.prototype.sckconnect = function () {
    var c = this.console;

    c.connected = true;
    c.setPrompt("connected");

    if (c.onconnected)
        c.onconnected();
}


/** Connection shutdown event handler. */
MrpConsole.prototype.sckclosed = function () {
    var c = this.console;

    console.log("socket closed");

    c.reset();

    if (c.onclosed)
        c.onclosed();
}


/** Socket error event handler. */
MrpConsole.prototype.sckerror = function (e) {
    var c = this.console;

    c.reset();

    if (c.onerror)
        c.onerror();

    if (c.onclosed)
        c.onclosed();
}


/** Socket message event handler. */
MrpConsole.prototype.sckmessage = function (message) {
    var c   = this.console;
    var msg = JSON.parse(message.data);

    if (msg.prompt)
        c.setPrompt(msg.prompt);
    else {
        if (msg.output) {
            c.append(msg.output);
        }
    }
}


/** Send a request to the server. */
MrpConsole.prototype.send_request = function (req) {
    var sck = this.sck;

    sck.send(JSON.stringify(req));
}


/** Process a command entered by the user. */
MrpConsole.prototype.processCmd = function (cmd) {
    var c, l, cb, args;

    for (c in this.commands) {
        l  = c.length;
        cb = this.commands[c];
        if (cmd.substring(0, l) == c && (cmd.length == l ||
                                         cmd.substring(l, l + 1) == ' ')) {
            args = cmd.split(' ').splice(1);

            this.commands[c](args);

            return;
        }
    }

    this.append(this.prompt + "> " + cmd + "\n");
    this.send_request({ input: cmd });

    if (cmd == 'help') {
        if (this.commands && Object.keys(this.commands).length > 0) {
            this.append("Web console commands:\n");
            for (c in this.commands) {
                this.append("    " + c + "\n");
            }
        }
        else
            this.append("No Web console commands.");
    }
}


/** Determine a WebSocket URI based on an HTTP URI. */
MrpConsole.prototype.socketUri = function (http_uri) {
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
