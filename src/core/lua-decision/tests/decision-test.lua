--[[
for n in pairs(_G) do
    print(n)
end
--]]

rows = {{key="a",value="A"},{key="b",value="B"},{key="c",value="C"}}

print("rows[2].value="..rows[2].value)

print("mdb.string="..mdb.string)

mdb.table {
    name    = "amb_values",
    index   = {"key"},
    columns = {{"key", mdb.string, 16}, {"value", mdb.floating}}
}

print("mdb.amb_values="..tostring(mdb.amb_values))

index = "{"

for _, k in ipairs(mdb.amb_values.index) do
    index = index.." "..k
end

index = index.." }"


coldefs = "{"

for _, cd in ipairs(mdb.amb_values.columns) do
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

print("mdb.amb_values.name="..mdb.amb_values.name)
print("mdb.amb_values.index="..index)
print("mdb.amb_values.columns="..coldefs)

mdb.amb_values[1] = { key = "foo", value = 3.1415 }

mdb.select {
           name = "speed",
           table = "amb_values",
           columns = {"value"},
           condition = "key = 'speed'"
}

print("mdb.speed.statement="..mdb.speed.statement)

q = mdb.speed[0]

mdb.speed:update()


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


foo = volume.speed_adjust

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
print("volume.speed_adjust "..type(volume.speed_adjust))
print("builtin.method.my_update_func "..type(builtin.method.my_update_func))

-- builtin.my_update_func()
print("update returned "..foo:update("Hello world"))
-- volume.speed_adjust=nil
-- foo=nil
collectgarbage()
print("done");
