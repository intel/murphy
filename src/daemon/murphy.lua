
--[[
# This section up variables for declaring
#   - required and optional plugins to load,
#   - application classes,
#   - zones (and zone attributes),
#   - resources
#
# See the documentation for more details about what these are and how
# they affect policy decision making:
#   - https://01.org/murphy
#   - https://01.org/murphy/documentation/audio-management-murphy
--]]

-- plugins to load
PLUGINS = {
   -- required ones
   { plugin = 'resource-native' },
   { plugin = 'domain-control'  },

   -- optional ones
   --   NOTES: telnet debug console, DO NOT use in a production environment !!!
   { plugin = 'console'      , optional = true },
   { plugin = 'glib'         , optional = true },
   { plugin = 'resource-dbus', optional = true, args = {
        dbus_bus = 'session',
        dbus_service = 'org.Murphy',
        dbus_track = true,
        default_zone = 'driver',
        default_class = 'implicit' } },
   { plugin = 'resource-wrt', optional = true, args = {
        -- sslcert = 'src/plugins/resource-wrt/resource.crt',
        -- sslpkey = 'src/plugins/resource-wrt/resource.key'
        address = 'wsck:127.0.0.1:4000/murphy',
        httpdir = 'src/plugins/resource-wrt' } },
   { plugin = 'domain-control', optional = true, instance = 'wrt-export',
     args = {
        external_address = '',
        internal_address = '',
        wrt_address = 'wsck:127.0.0.1:5000/murphy',
        httpdir = 'src/plugins/domain-control' } },
}

-- application classes
APPLICATION_CLASSES = {
   interrupt = { priority = 99, order = 'fifo' },
   navigator = { priority =  4, order = 'fifo' },
   phone     = { priority =  3, order = 'lifo' },
   game      = { priority =  2, order = 'lifo' },
   player    = { priority =  1, order = 'lifo' },
   implicit  = { priority =  0, order = 'lifo' },
}

-- zone attributes and zones
ZONE_ATTRIBUTES = {
   type     = { mdb.string, 'common'  , 'rw' },
   location = { mdb.string, 'anywhere', 'rw' },
}

ZONES = {
   driver     = { type = 'common' , location = 'front-left'  },
   passenger1 = { type = 'private', location = 'front-right' },
   passenger2 = { type = 'private', location = 'back-left'   },
   passenger3 = { type = 'private', location = 'back-center' },
   passenger4 = { type = 'private', location = 'back-right'  },
}

-- resources
RESOURCES = {
   audio_playback  = { shareable = true, attributes = {
                          role   = { mdb.string, 'music'    , 'rw' },
                          pid    = { mdb.string, '<unknown>', 'rw' },
                          policy = { mdb.string, 'relaxed'  , 'rw' },
                          name   = { mdb.string, '<unknown>', 'rw' } } },
   audio_recording = { shareable = false, attributes = {
                          role   = { mdb.string, 'music'    , 'rw' },
                          pid    = { mdb.string, '<unknown>', 'rw' },
                          policy = { mdb.string, 'relaxed'  , 'rw' },
                          name   = { mdb.string, '<unknown>', 'rw' } } },
   video_playback  = { shareable = false },
   video_recording = { shareable = false },
}


-- # get the Murphy singleton instance to access the Murphy Lua bindings
m = murphy.get()

-- # load our helper utility library
m:include('murphy-utils.lua')

-- # load plugins, declare application classes, zones, and resources
load_plugins(m, PLUGINS)
declare_application_classes(m, APPLICATION_CLASSES)
declare_zones(m, ZONE_ATTRIBUTES, ZONES)
declare_resources(m, RESOURCES)
