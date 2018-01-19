m = murphy.get()
m:open_lualib('table')

-- #
-- # declare mandatory and optional plugins to load
-- #
PLUGINS = {
   -- mandatory plugins, failing to load will abort startup
   { name = 'dbus'           , optional = false },
   { name = 'resource-native', optional = false },
   { name = 'domain-control' , optional = false },

   -- !!! NOTES: debug telnet console, you DON'T want this in production builds
   { name = 'console'        , optional = true  },

   -- optional plugins, failing to load is not considered an error
   { name = 'glib'           , optional = true  },
   { name = 'resource-dbus'  , optional = true,
     args = {
        dbus_bus = 'session',
        dbus_service = 'org.Murphy',
        dbus_track = true,
        default_zone = 'driver',
        default_class = 'implicit'
     }
   },
   { name = 'resource-wrt'   , optional = true,
     args = {
        address = 'wsck:127.0.0.1:4000/murphy',
        httpdir = 'src/plugins/resource-wrt',
--        sslcert = 'src/plugins/resource-wrt/resource.crt',
--        sslpkey = 'src/plugins/resource-wrt/resource.key'
     }
   },
   { name = 'domain-control' , optional = true,
     instance = 'wrt-export', args = {
        external_address = '',
        internal_address = '',
        wrt_address = 'wsck:127.0.0.1:5000/murphy',
        httpdir = 'src/plugins/domain-control'
     },
   },
}

-- #
-- # declare application classes
-- #
APPLICATION_CLASSES = {
   interrupt = { priority = 99, modal = true , share = false, order = 'fifo' },
   navigator = { priority =  4, modal = false, share = true , order = 'fifo' },
   phone     = { priority =  3, modal = false, share = true , order = 'lifo' },
   game      = { priority =  2, modal = false, share = true , order = 'lifo' },
   player    = { priority =  1, modal = false, share = true , order = 'lifo' },
   implicit  = { priority =  0, modal = false, share = true , order = 'lifo' },
}

-- #
-- # declare resource/application (conflict) zone attributes and zones
-- #
ZONE_ATTRIBUTES = {
   type     = { mdb.string, 'common'  , 'rw' },
   location = { mdb.string, 'anywhere', 'rw' },
}

ZONES = {
   driver     = { type = 'common' , location = 'front-left'  },
   passenger1 = { type = 'private', location = 'front-right' },
   passenger2 = { type = 'private', location = 'back-left'   },
   passenger3 = { type = 'private', location = 'back-right'  },
   passenger4 = { type = 'private', location = 'back-left'   },
}

-- #
-- # declare resources
-- #
RESOURCES = {
   audio_playback  = { shareable = true, attributes = {
                          role   = { mdb.string, 'music'    , 'rw' },
                          pid    = { mdb.string, '<unknown>', 'rw' },
                          policy = { mdb.string, 'relaxed'  , 'rw' } } },
   audio_recording = { shareable = false, attributes = {
                          role   = { mdb.string, 'music'    , 'rw' },
                          pid    = { mdb.string, '<unknown>', 'rw' },
                          policy = { mdb.string, 'relaxed'  , 'rw' } } },
   video_playback  = { shareable = false },
   video_recording = { shareable = false },
}

-- ##########################################################################

-- # load plugins
for _, p in ipairs(PLUGINS) do
   args = { p.name }
   if p.instance then
      table.insert(args, p.instance)
   end
   if p.args then
      table.insert(args, p.args)
   end

   if not p.optional then
      if not m:plugin_exists(p.name) then
         m:error('mandatory plugin \''..p.name..'\'not found')
      else
         m:info('* loading plugin \''..p.name..'\'...')
         m:load_plugin(table.unpack(args))
      end
   else
         m:info('* trying to load optional plugin \''..p.name..'\'...')
         m:try_load_plugin(table.unpack(args))
   end
end

-- # declare application classes
for name, adef in pairs(APPLICATION_CLASSES) do
   m:info('creating application class \'' .. name .. '\'...')
   adef.name = name
   application_class(adef)
end

-- # declare zones attribtues and zones
zone.attributes(ZONE_ATTRIBUTES)

for name, def in pairs(ZONES) do
   m:info('creating zone \'' .. name .. '\'...')
   zdef = { name = name, attributes = def }
   zone(zdef)
end

-- # declare resources
for name, rdef in pairs(RESOURCES) do
   m:info('creating resource class \'' .. name .. '\'...')
   rdef.name = name
   resource.class(rdef)
end
