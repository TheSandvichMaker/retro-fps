-- scans a shader file for a @metashader tag, and parses whatever table follows as a lua table and
-- returns it, with the prelude and source members on the table set to the HLSL before and after
-- the table.

-- returns: shader bundle, or nil if there is no @metashader block
function parse_metashader_block(shader_file_name, shader_text)
	assert(shader_file_name and type(shader_file_name) == "string", "Called parse_metashader_block without providing a shader filename, or it's not a string, that's not allowed")
	assert(shader_text      and type(shader_text)      == "string", "Called parse_metashader_block without providing shader source code, or it's not a string, what do you expect to happen?")

	local i, j = string.find(shader_text, "@metashader")
	local metashader_tag_start = i

	local result = nil

	if i ~= nil then
		-- skip whitespace
		i, j = string.find(shader_text, "^%s*", j + 1)

		-- now the table should follow
		local table_start, table_end = string.find(shader_text, "^%b{}", j + 1)
		assert(table_start ~= nil, "A @metashader tag needs to be followed by a set of balanced open and closing curly braces defining a valid lua table!")

		local the_table = string.sub(shader_text, table_start, table_end)

		local f, err = loadstring("return " .. the_table)

		if not f then
			error(shader_file_name .. ": Failed to parse @metashader block:\n" .. err);
		end

		result = f()

		assert(result         ~= nil,     shader_file_name .. ": The metashader block failed to parse into a valid lua table!")
		assert(type(result)   == "table", shader_file_name .. ": The metashader block failed to parse into a lua table, it parsed into a " .. type(result) .. " instead, WHAT!?")
		assert(result.prelude == nil,     shader_file_name .. ": A metashader block can't have a prelude entry")
		assert(result.source  == nil,     shader_file_name .. ": A metashader block can't have a source entry")

		result.prelude = string.sub(shader_text, 1, metashader_tag_start - 1)
		result.source  = string.sub(shader_text, table_end + 1)

		-- sanity check that there was only one metashader block
		i, j = string.find(shader_text, "@metashader", j + 1)

		if i then
			error("A shader can only have one @metashader block! (error while parsing '" .. shader_file_name .. "')")
		end
	end

	if result ~= nil then
		print("Successfully parsed @metashader table from " .. shader_file_name)
	end

	return result
end