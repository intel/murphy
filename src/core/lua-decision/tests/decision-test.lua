--[[
for n in pairs(_G) do
    print(n)
end
--]]

print_table = function (tbl)
   print("-------- mdb start -------");
   for kn, n in pairs(tbl) do
      if type(n) ~= "table" then
	 if type(n) == "function" then
	    print(kn.."=<function>")
	 else
	    print(kn.."="..n)
	 end
      else
	 print("'"..kn.."' table ")
	 for km, m in pairs(n) do
	    if type(n) == "function" then
	       print("   "..km.."=<function>")
	    elseif type(n) == "table" then
	       print("   '"..km.."' table")
	    else
	       print("   "..km.."="..m)
	    end
	 end
      end
   end
   print("--------- mdb end --------");
end

print_table(mdb)

rows = {{key="a",value="A"},{key="b",value="B"},{key="c",value="C"}}

print("rows[2].value="..rows[2].value)

print("mdb.string="..mdb.string)

mdb.table {
    name    = "amb",
    index   = {"key"},
    columns = {{"key", mdb.string, 16}, {"value", mdb.floating}}
}

print("mdb.amb="..tostring(mdb.table.amb))

index = "{"

for _, k in ipairs(mdb.table.amb.index) do
    index = index.." "..k
end

index = index.." }"


coldefs = "{"

for _, cd in ipairs(mdb.table.amb.columns) do
    local name, type, length

    for i, v in ipairs(cd) do
        if i == 1 then  name = v   end
        if i == 2 then  type = v   end
        if i == 3 then  length = v end
    end

    coldefs = coldefs.." {"..name..","..type

    if length then
        coldefs = coldefs..","..length
    end

    coldefs = coldefs.."}"
end

coldefs = coldefs.." }"

print("mdb.table.amb.name="..mdb.table.amb.name)
print("mdb.table.amb.index="..index)
print("mdb.table.amb.columns="..coldefs)

mdb.table.amb[1] = { key = "foo", value = 3.1415 }

mdb.select {
           name = "speed",
           table = "amb",
           columns = {"value"},
           condition = "key = 'speed'"
}

--[[
print("mdb.select.speed.statement="..mdb.select.speed.statement)

q = mdb.select.speed[0]

mdb.select.speed:update()

print_table(mdb)

--]]

element.lua {
   name    = "speed2volume",
   inputs  = { "bar", foo = mdb.select.speed, param = 5 }, 
   outputs = {  mdb.table { name = "speedvol",
			    index = {"zone", "device"},
			    columns = {{"zone", mdb.string, 16},
				       {"device", mdb.string, 16},
				       {"value", mdb.floating}}
			   }
	     },
   update  = function(self)
		print("*** element "..self.name.." update function called")
	     end
}

-- print_table(element)

element.lua.speed2volume.inputs.bar = mdb.select {name = "rpm",
						  table = "amb",
						  columns = {"value"},
						  condition = "key = 'rpm'"}

element.lua.speed2volume:update()
print("speed2volume.inputs.param="..element.lua.speed2volume.inputs.param)
-- print("speed2volume.inputs.foo[0].value="..element.lua.speed2volume.inputs.foo[0].value)

volume.limit {
    name = "speed_adjust",
    devices = {"speaker", "headphone", "headset"},
    limit = -0.5,
    extra = 2468,
    update = builtin.method.my_update_func

    --[[
    update = function(self, arg)
                 print("*** lua update function arg="..arg.." extra="..self.extra)
                 return 987.0
             end
    --]]
}


foo = volume.limit.speed_adjust

for k,v in pairs(foo) do
    print(k..": "..type(v))
end

a = pairs(foo)
print("a "..type(a))

print("limit "..foo.limit)

foo.limit = -31.2
foo.yoyo = "a"

print("limit "..foo.limit)
print("type "..foo.type)
print("yoyo "..foo.yoyo)
print("extra "..foo.extra)
print("volume.limit.speed_adjust "..type(volume.limit.speed_adjust))
print("builtin.method.my_update_func "..type(builtin.method.my_update_func))

-- builtin.my_update_func()
print("update returned "..foo:update("Hello world"))
-- volume.speed_adjust=nil
-- foo=nil
collectgarbage()
print("done");
