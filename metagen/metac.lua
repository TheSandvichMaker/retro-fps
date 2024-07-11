function process_c_file(file)
	local f    = io.open(file, "r")
	local text = f:read("*all")

    local meta_objects = {}

	local i, j = 0, 0

    while true do
        i, j = string.find(text, "meta_struct", j + 1)

        if i ~= nil then
            i, j, struct_name = string.find(text, "^%s+typedef%s+struct%s+([%w_]+)%s*", j + 1)
            assert(struct_name ~= nil, "Failed to parse meta_struct name")

            print("Parsing meta_struct '" .. struct_name .. "'")

            i, j, struct_body = string.find(text, "(%b{})", j + 1)
            assert(struct_body ~= nil, "Failed to parse struct body")

            local p, q = 1, 1

            local members = {}

            while true do
                p, q, meta, member_type, member_name = 
                    string.find(struct_body, "(meta%b())%s+([%w_]+)%s+([%w_]+);", q + 1)

                if p == nil then
                    break
                end

                local meta_as_table = nil

                if meta ~= nil then
                    local meta_loadstring = "return {" .. meta:sub(6, -2) .. "}"
                    meta_as_table = loadstring(meta_loadstring)()
                end

                table.insert(members, {
                    type = member_type,
                    name = member_name,
                    meta = meta_as_table,
                })
            end

            table.insert(meta_objects, {
                kind    = "meta_struct",
                members = members,
            })
        else
            break
        end
    end

    f:close()
end
