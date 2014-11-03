m = murphy.get()

-- try loading console plugin
m:try_load_plugin('console')

--[[
m:try_load_plugin('console', 'dbusconsole' , {
    address = 'dbus:[session]@org.Murphy/console'
})
--]]

m:try_load_plugin('console', 'webconsole', {
                              address = 'wsck:127.0.0.1:3000/murphy',
                              httpdir = 'src/plugins/console',
--                              sslcert = 'src/plugins/console/console.crt',
--                              sslpkey = 'src/plugins/console/console.key'
         })

m:try_load_plugin('systemd')

-- load a test plugin
if m:plugin_exists('test.disabled') then
    m:load_plugin('test', {
                       string2  = 'this is now string2',
                       boolean2 = true,
                       int32 = -981,
                       double = 2.73,
                       object = {
                           foo = 1,
                           bar = 'bar',
                           foobar = 3.141,
                           barfoo = 'bar foo',
                           array = { 'one', 'two', 'three',
                                     { 1, 'two', 3, 'four' } },
                           yees = true,
                           noou = false
                       }
                 })
--    m:load_plugin('test', 'test2')
--    m:info("Successfully loaded two instances of test...")
end

-- load the dbus plugin if it exists
-- if m:plugin_exists('dbus') then
--     m:load_plugin('dbus')
-- end

-- load glib plugin, ignoring any errors
m:try_load_plugin('glib')

-- load the native resource plugin
if m:plugin_exists('resource-native') then
    m:load_plugin('resource-native')
    m:info("native resource plugin loaded")
else
    m:info("No native resource plugin found...")
end

-- load the dbus resource plugin
if m:plugin_exists('resource-dbus') then
    m:try_load_plugin('resource-dbus', {
        dbus_bus = "system",
        dbus_service = "org.Murphy",
        dbus_track = true,
        default_zone = "driver",
        default_class = "implicit"
      })
    m:info("dbus resource plugin loaded")
else
    m:info("No dbus resource plugin found...")
end

-- load the WRT resource plugin
if m:plugin_exists('resource-wrt') then
    m:try_load_plugin('resource-wrt', {
                          address = "wsck:127.0.0.1:4000/murphy",
                          httpdir = "src/plugins/resource-wrt",
--                          sslcert = 'src/plugins/resource-wrt/resource.crt',
--                          sslpkey = 'src/plugins/resource-wrt/resource.key'
                      })
else
    m:info("No WRT resource plugin found...")
end

-- load the domain control plugin if it exists
if m:plugin_exists('domain-control') then
    m:load_plugin('domain-control')
else
    m:info("No domain-control plugin found...")
end

-- load the domain control plugin if it exists
if m:plugin_exists('domain-control') then
    m:try_load_plugin('domain-control', 'wrt-export', {
        external_address = '',
        internal_address = '',
        wrt_address = "wsck:127.0.0.1:5000/murphy",
        httpdir     = "src/plugins/domain-control"
    })
else
    m:info("No domain-control plugin found...")
end


-- define application classes
application_class { name="interrupt", priority=99, modal=true , share=false, order="fifo" }
application_class { name="navigator", priority=4 , modal=false, share=true , order="fifo" }
application_class { name="phone"    , priority=3 , modal=false, share=true , order="lifo" }
application_class { name="game"     , priority=2 , modal=false, share=true , order="lifo" }
application_class { name="player"   , priority=1 , modal=false, share=true , order="lifo" }
application_class { name="implicit" , priority=0 , modal=false, share=true , order="lifo" }

-- define zone attributes
zone.attributes {
    type = {mdb.string, "common", "rw"},
    location = {mdb.string, "anywhere", "rw"}
}

-- define zones
zone {
     name = "driver",
     attributes = {
         type = "common",
         location = "front-left"
     }
}

zone {
     name = "passanger1",
     attributes = {
         type = "private",
         location = "front-right"
     }
}

zone {
     name = "passanger2",
     attributes = {
         type = "private",
         location = "back-left"
     }
}

zone {
     name = "passanger3",
     attributes = {
         type = "private",
         location = "back-right"
     }
}

zone {
     name = "passanger4",
     attributes = {
         type = "private",
         location = "back-left"
     }
}


-- define resource classes
resource.class {
     name = "audio_playback",
     shareable = true,
     attributes = {
         role = { mdb.string, "music", "rw" },
         pid = { mdb.string, "<unknown>", "rw" },
         policy = { mdb.string, "relaxed", "rw" }
     }
}

resource.class {
     name = "audio_recording",
     shareable = false,
     attributes = {
         role = { mdb.string, "music", "rw" },
         pid = { mdb.string, "<unknown>", "rw" },
         policy = { mdb.string, "relaxed", "rw" }
     }
}

resource.class {
     name = "video_playback",
     shareable = false,
}

resource.class {
     name = "video_recording",
     shareable = false
}

-- test for creating selections
mdb.select {
           name = "audio_owner",
           table = "audio_playback_owner",
           columns = {"application_class"},
           condition = "zone_name = 'driver'",
}

element.lua {
   name    = "speed2volume",
   inputs  = { owner = mdb.select.audio_owner, param = 5 },
   outputs = {  mdb.table { name = "speedvol",
			    index = {"zone", "device"},
			    columns = {{"zone", mdb.string, 16},
				       {"device", mdb.string, 16},
				       {"value", mdb.floating}},
                            create = true
			   }
	     },
   update  = function(self)
                if (self.inputs.owner.single_value) then
                   print("*** element "..self.name.." update "..
                          self.inputs.owner.single_value)
                else
                   print("*** element "..self.name.." update <nil>")
                end
	     end
}


--

json = m:JSON({ a = 'foo', b = 'bar', foobar = { 1, 2, 3, 5, 6 } })

print(tostring(json))


function connect_cb(self, peer, data)
    print('incoming connection from ' .. peer .. ' on ' .. tostring(self))
    accepted = self:accept()
    print('accepted: ' .. tostring(accepted))
end

function closed_cb(self, error, data)
    print('connection closed by peer')
end

function recv_cb(self, msg, data)
    print('got message ' .. tostring(msg))
end

t = m:Transport({ connect = connect_cb,
                  closed  = closed_cb,
                  recv    = recv_cb,
                  data    = 'foo',
                  address = 'wsck:127.0.0.1:18081/ico_syc_protocol' })

print(tostring(t))

t:listen()

print(tostring(t))

print(t.accept)
print(t.recv)

function test_rs_create(iterations)
    resourceHandler = function (rset) print(rset) end

    for i = 1, iterations do
        r = m:ResourceSet({
            zone = "driver",
            callback = resourceHandler,
            application_class = "player"
        })

        if r then

            r:addResource({
                resource_name = "audio_playback",
                mandatory = true
            })

            r.resources.audio_playback.attributes.pid = tostring(i)

            print("pid: " .. r.resources.audio_playback.attributes.pid)
            r:acquire()
            r:release()
        end
        r = nil
    end
end
