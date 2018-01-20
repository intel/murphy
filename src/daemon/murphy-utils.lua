-- #
-- # a small set of utilities for the main murphy Lua configuration
-- #

-- # make built-in libraries we rely on available
m:open_lualib('table')
m:open_lualib('os')

function plugin(name, optional, ...)
   p = { plugin = name, optional = optional }
   for _, v in ipairs({select(1, ...)}) do
      if type(v) == type('') then
         if not p.instance then
            p.instance = v
         else
            m:error('REQUIRED_PLUGIN: multiple instance names for '..name)
            os.exit(1)
         end
      elseif type(v) == type({}) then
         if not p.args then
            p.args = v
         else
            m:error('REQUIRED_PLUGIN: multiple argument sets given for '..name)
            os.exit(1)
         end
      else
         m:error('REQUIRED_PLUGIN: invalid argument ' .. str(v) .. ' for '..name)
      end
   end
   return p
end

function REQUIRED_PLUGIN(name, ...)
   return plugin(name, false, select(1, ...))
end

function OPTIONAL_PLUGIN(name, ...)
   return plugin(name, true, select(1, ...))
end



-- # load a given set of plugins
function load_plugins(m, plugins)
   for _, p in ipairs(plugins) do
      args = { p.plugin }
      if p.instance then
         table.insert(args, p.instance)
      end
      if p.args then
         table.insert(args, p.args)
      end

      if not p.optional then
         if not m:plugin_exists(p.plugin) then
            m:error('required plugin \'' .. p.plugin .. '\'not found')
            os.exit(1)
         else
            m:info('* loading plugin \'' .. p.plugin .. '\'...')
            m:load_plugin(table.unpack(args))
         end
      else
         m:info('* loading optional plugin \'' .. p.plugin .. '\'...')
         m:try_load_plugin(table.unpack(args))
      end
   end
end

-- # declare the given set of application classes
function declare_application_classes(m, classes)
   for name, adef in pairs(classes) do
      m:info('* creating application class \'' .. name .. '\'...')
      adef.name  = name
      -- fill in deprecated/unused but still mandatory fields
      adef.modal = false
      adef.share = true
      application_class(adef)
   end
end

-- # declare zones attribtues and zones
function declare_zones(m, attributes, zones)
   zone.attributes(attributes)
   for name, def in pairs(zones) do
      m:info('* creating zone \'' .. name .. '\'...')
      zdef = { name = name, attributes = def }
      zone(zdef)
   end
end

-- # declare resources
function declare_resources(m, resources)
   for name, rdef in pairs(resources) do
      m:info('* creating resource class \'' .. name .. '\'...')
      rdef.name = name
      resource.class(rdef)
   end
end
