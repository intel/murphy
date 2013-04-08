#!/usr/bin/env python

# Copyright (c) 2012, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#  * Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of Intel Corporation nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# This script is an example on how to use the Murphy D-Bus resource API.


from __future__ import print_function

import dbus
import gobject
import glib
import sys
import fcntl
import os
from dbus.mainloop.glib import DBusGMainLoop

from itertools import combinations
from random import choice

USAGE = """
Available commands:

help

createSet

deleteSet <set>
changeSetClass <set> <class>
acquireSet <set>
releaseSet <set>
showSet <set>

createResource <set> <type>

deleteResource <set> <resource>
changeResource <set> <resource> <attribute> <value>
showResource <set> <resource>

quit"""

manager = None
bus = None
mainloop = None
interactive = None
n_iterations = None
limit = None

# mapping from numbers to object paths
rsets = {}
resources = {}


# pretty printing of D-Bus properties

def pretty_str_dbus_value(val, level=0, suppress=False):
    if type(val) == dbus.Array:
        return pretty_str_dbus_array(val, level)
    elif type(val) == dbus.Dictionary:
        return pretty_str_dbus_dict(val, level)
    else:
        s = ""
        if not suppress:
            s += level * "\t"
        if type(val) == dbus.Boolean:
            if val:
                s += "True"
            else:
                s += "False"
        else:
            s += str(val)
        return s


def pretty_str_dbus_array(arr, level=0):
    prefix = level * "\t"
    s = "[\n"
    for v in arr:
        s += pretty_str_dbus_value(v, level+1)
        s += "\n"
    s += prefix + "]"
    return s


def pretty_str_dbus_dict(d, level=0):
    prefix = level * "\t"
    s = "{\n"
    for k, v in d.items():
        s += prefix + "\t"
        s += str(k) + ": "
        s += pretty_str_dbus_value(v, level+1, True)
        s += "\n"
    s += prefix + "}"
    return s


# methods for getting a D-Bus object from path

def get_rset(path):
    set_obj = bus.get_object('org.Murphy', path)
    return dbus.Interface(set_obj, dbus_interface='org.murphy.resourceset')


def get_res(path):
    resource_obj = bus.get_object('org.Murphy', path)
    return dbus.Interface(resource_obj, dbus_interface='org.murphy.resource')


# prompt handling

prompt_needed = True

def add_prompt():
    global prompt_needed
    global interactive

    if prompt_needed and interactive:
        print("")
        print("> ", end="")
        sys.stdout.flush()
        prompt_needed = False


def add_prompt_later():
    global prompt_needed
    global interactive

    if not interactive:
        return

    prompt_needed = True

    # add in idle loop so that we first process all queued signals
    glib.idle_add(add_prompt)


# signal handlers

def resource_handler(prop, value, path):

    res = int(path.split("/")[-1]) # the resource number
    set = int(path.split("/")[-2]) # the set number

    print("(%d/%d) property %s -> %s" % (set, res, str(prop), pretty_str_dbus_value(value)))
    add_prompt_later()


def rset_handler(prop, value, path):

    set = int(path.split("/")[-1]) # the set number

    print("(%d) property %s -> %s" % (set, str(prop), pretty_str_dbus_value(value)))
    add_prompt_later()


def mgr_handler(prop, value, path):

    print("(manager) property %s -> %s" % (str(prop), pretty_str_dbus_value(value)))
    add_prompt_later()


# helper functions

def get_set_path(setId):
    try:
        return rsets[setId]
    except:
        return None


def get_res_path(setId, resId):
    try:
        return resources[(setId, resId)]
    except:
        return None


def get_resource_set(set):
    try:
        id = int(set)
    except ValueError:
        print("ERROR: wrong resource set id type")
        return None

    set_path = get_set_path(int(set))

    if set_path:
        return get_rset(set_path)
    else:
        print("ERROR: resource set doesn't exist")
    return None


def get_resource(set, res):
    try:
        id_s = int(set)
        id_r = int(res)
    except ValueERROR:
        print("ERROR: wrong resource or resource set id type")
        return None

    res_path = get_res_path(int(set), int(res))

    if res_path:
        return get_res(res_path)
    else:
        print("ERROR: resource doesn't exist")
    return None


# UI functions

def help():
    print(USAGE)


def createSet():
    try:
        set_path = manager.createResourceSet()
        id = int(set_path.split("/")[-1]) # the set number
        rsets[id] = set_path
        rset = get_rset(set_path)
        if interactive:
            rset.connect_to_signal("propertyChanged", rset_handler, path_keyword='path')
        print("(%s) Created resource set" % str(id))
        return id
    except:
        print("ERROR: failed to create a new resource set")


def deleteSet(set):

    rset = get_resource_set(set)

    if rset:
        try:
            rset.delete()
            del rsets[int(set)]
            print("(%s) Deleted resource set" % set)
        except:
            print("ERROR (%s): failed to delete resource set" % set)


def createResource(set, rType):
    rset = get_resource_set(set)

    if rset:
        try:
            res_path = rset.addResource(rType)
            res = int(res_path.split("/")[-1]) # the resource id number
            resources[(int(set), res)] = res_path

            resource = get_res(res_path)
            if interactive:
                resource.connect_to_signal("propertyChanged", resource_handler, path_keyword='path')
            print("(%s/%d) added resource '%s'" % (set, res, rType))
            return res
        except:
            print("ERROR (%s): failed to add resource to resource set" % set)


def changeSetClass(set, klass):

    rset = get_resource_set(set)

    if rset:
        try:
            rset.setProperty("class", dbus.String(klass, variant_level=1))
            print("(%s) changed the application class to %s" % (set, klass))
        except:
            print("ERROR (%s): failed to change resource set class to '%s'" % (set, klass))


def acquireSet(set):

    rset = get_resource_set(set)

    if rset:
        try:
            rset.request()
            print("(%s) asked (asynchronously) to acquire" % set)
        except:
            print("ERROR (%s): failed to acquire resource set" % set)


def releaseSet(set):

    rset = get_resource_set(set)

    if rset:
        try:
            rset.release()
            print("(%s) asked (asynchronously) to release" % set)
        except:
            print("ERROR (%s): failed to release resource set" % set)


def showSet(set):

    rset = get_resource_set(set)

    if rset:
        try:
            print("(%s)" % set)
            values = rset.getProperties()
            print(pretty_str_dbus_value(values))
        except:
            print("ERROR (%s): failed to query properties of resource set" % set)


def deleteResource (set, resource):

    res = get_resource(set, resource)

    if res:
        try:
            res.delete()
            del resources[(int(set), int(resource))]
            print("(%s/%s) deleted resource" % (set, resource))
        except:
            print("ERROR (%s/%s): failed to delete resource" % (set, resource))


def changeResource(set, resource, attribute, value):

    res = get_resource(set, resource)

    if res:
        try:
            if attribute == "shared" or attribute == "mandatory":
                val = False
                if value == "True" or value == "1":
                    val = True

                res.setProperty(attribute, dbus.Boolean(val, variant_level=1))
                print("(%s/%s) set attribute '%s' to '%s'" % (set, resource, attribute, val))
            else:
                attrs = res.getProperties()["attributes"]

                if attribute in attrs:

                    if type(attrs[attribute]) is dbus.String:
                        attrs[attribute] = dbus.String(value)
                    elif type(attrs[attribute]) is dbus.Int32:
                        attrs[attribute] = dbus.Int32(value)
                    elif type(attrs[attribute]) is dbus.UInt32:
                        attrs[attribute] = dbus.UInt32(value)
                    elif type(attrs[attribute]) is dbus.Double:
                        attrs[attribute] = dbus.Double(value)

                    res.setProperty("attributes_conf", attrs)
                    print("(%s/%s) set attribute '%s' to '%s'" %
                                    (set, resource, attribute, str(attrs[attribute])))
                else:
                    print("ERROR (%s/%s): attribute '%s' not supported in resource" % (set, resource, attribute))
        except dbus.DBusException as e:
            print("ERROR (%s/%s): failed to set attribute '%s' in resource: %s" % (set, resource, attribute, e))
        except:
            print("ERROR (%s/%s): failed to convert value type to D-Bus" % (set, resource))



def showResource(set, resource):

    res = get_resource(set, resource)

    if res:
        try:
            print("(%s/%s)" % (set, resource))
            values = res.getProperties()
            print(pretty_str_dbus_value(values))
        except dbus.DBusException, e:
            print("ERROR (%s/%s): failed to query properties of resource: %s" % (set, resource, e))


def stdin_cb(fd, condition):
    data = fd.read()
    tokens = data.split()

    set = None
    resource = None

    if len(tokens) == 0 or len(tokens) > 5:
        return True

    if tokens[0] == "createSet" and len(tokens) == 1:
        createSet()
    elif tokens[0] == "deleteSet" and len(tokens) == 2:
        deleteSet(*tokens[1:])
    elif tokens[0] == "createResource" and len(tokens) == 3:
        createResource(*tokens[1:])
    elif tokens[0] == "changeSetClass" and len(tokens) == 3:
        changeSetClass(*tokens[1:])
    elif tokens[0] == "acquireSet" and len(tokens) == 2:
        acquireSet(*tokens[1:])
    elif tokens[0] == "releaseSet" and len(tokens) == 2:
        releaseSet(*tokens[1:])
    elif tokens[0] == "showSet" and len(tokens) == 2:
        showSet(*tokens[1:])
    elif tokens[0] == "deleteResource" and len(tokens) == 3:
        deleteResource(*tokens[1:])
    elif tokens[0] == "changeResource" and len(tokens) == 5:
        changeResource(*tokens[1:])
    elif tokens[0] == "showResource" and len(tokens) == 3:
        showResource(*tokens[1:])
    elif tokens[0] == "quit":
        mainloop.quit()
        return False
    else:
        help()

    add_prompt_later()

    return True


def fuzz_test():
    """ randomly create, acquire, release and destroy resource sets """

    global rsets
    global n_iterations

    resource_names = [ "audio_playback", "audio_recording" ]
    operations = [ "create", "delete", "acquire", "release" ]
    attributes={"pid":range(1, 1000),"role":["music","navigator","game"],"policy":["relaxed","strict"]}
    coin = [True, False]

    print("fuzz_test, left", n_iterations, "iterations")

    if n_iterations > 0:
        n_iterations = n_iterations - 1

        if len(rsets) == 0:
            # no sets, have to create
            op = "create"
        elif limit and len(rsets) >= limit:
            # limit was reached
            op = "delete"
        else:
            op = choice(operations)

        if op == "create":
            rset_id = createSet()

            if (rset_id != None):
                n_res = choice(range(1, len(resource_names)))
                res_choice = choice(list(combinations(resource_names, n_res)))

                print("created rset", str(rset_id))

                for res in res_choice:
                    res_id = createResource(str(rset_id), res)
                    print("resource", res)

                    # change some resources

                    # this many attributes
                    n_attrs = choice(range(1, len(attributes)))
                    attr_choice = choice(list(combinations(attributes.keys(), n_attrs)))

                    for attr in attr_choice:
                        values = attributes[attr]
                        value = choice(values)
                        changeResource(str(rset_id), str(res_id), attr, value)

                    changeResource(str(rset_id), str(res_id), "mandatory", str(choice(coin)));
                    changeResource(str(rset_id), str(res_id), "shared", str(choice(coin)));


        elif op == "delete":
            if len(rsets) > 0:
                id = choice(rsets.keys())
                print("deleting rset", id)
                deleteSet(str(id))

        elif op == "acquire":
            if len(rsets) > 0:
                id = choice(rsets.keys())
                print("acquiring rset", id)
                acquireSet(str(id))

        elif op == "release":
            if len(rsets) > 0:
                id = choice(rsets.keys())
                print("releasing rset", id)
                releaseSet(str(id))

        return True

    else:
        return False # do not call again


def main(args):
    global manager
    global bus
    global mainloop
    global interactive
    global n_iterations

    # D-Bus initialization

    DBusGMainLoop(set_as_default=True)
    mainloop = gobject.MainLoop()

    bus = dbus.SystemBus()

    if not bus:
        print("ERROR: failed to get system bus")
        exit(1)

    # Create the manager for handling resource sets.

    manager_obj = None

    # TODO: get service and manager path from command line?
    try:
        manager_obj = bus.get_object('org.Murphy', '/org/murphy/resource')
    except:
        pass

    if not manager_obj:
        print("ERROR: failed get Murphy resource manager object")
        exit(1)

    manager = dbus.Interface(manager_obj, dbus_interface='org.murphy.manager')

    if (len(args) > 0 and args[0] == "fuzz"):
        interactive = False
        n_iterations = 1000
        if (len(args) == 2):
            n_iterations = int(args[1])
            if (len(args) == 3):
                limit = int(args[2])
        glib.idle_add(fuzz_test)

    else:
        # interactive mode
        interactive = True
        manager.connect_to_signal("propertyChanged", mgr_handler, path_keyword='path')
        # make STDIN non-blocking
        fcntl.fcntl(sys.stdin.fileno(), fcntl.F_SETFL, os.O_NONBLOCK)
        # listen for user input
        glib.io_add_watch(sys.stdin, glib.IO_IN, stdin_cb)
        add_prompt()

    mainloop.run()

    # TODO: cleanup

main(sys.argv[1:])