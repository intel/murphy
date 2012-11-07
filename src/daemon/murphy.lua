m = murphy.get()

-- try loading console plugin
m:try_load_plugin('console')

-- load two instances of the test plugin
if m:plugin_exists('test') then
    m:load_plugin('test', 'test2', {
                       string2  = 'this is now string2',
                       boolean2 = true,
                       int32 = -981,
                       double = 2.73 })
    m:load_plugin('test', 'test5')
    m:info("Successfully loaded two instances of test...")
end

-- load the dbus plugin if it exists
if m:plugin_exists('dbus') then
    m:load_plugin('dbus')
end

-- load glib plugin, ignoring any errors
m:try_load_plugin('glib')

-- try loading the signalling plugin
m:try_load_plugin('signalling', { address = 'internal:signalling' })

-- load the native resource plugin
if m:plugin_exists('resource-native') then
    m:load_plugin('resource-native')
else
    m:info("No native resource plugin found...")
end

-- load the domain control plugin if it exists
if m:plugin_exists('domain-control') then
    m:load_plugin('domain-control')
else
    m:info("No domain-control plugin found...")
end

mdb.select {
           name = "speed",
           table = "audio_playback_owner",
           columns = {"application_class"},
           condition = "key = 'driver'"
}
