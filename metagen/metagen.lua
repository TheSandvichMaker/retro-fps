require "io"
require "os"
require "metagen/metashader"
require "metagen/emit"
require "metagen/metac"

src_directory         = "src"
shader_directory      = "src/shaders/"
output_directory_hlsl = "src/shaders/gen/"
output_directory_c    = "src/game/render/shaders/gen/"

-- @PlatformSpecific
os.execute("rmdir \"" .. output_directory_hlsl .. "\" /S /Q")
os.execute("rmdir \"" .. output_directory_c    .. "\" /S /Q")

if arg[1] == "clean" then
	os.exit(0)
end

-- @PlatformSpecific
os.execute("if not exist \"" .. output_directory_hlsl .. "\" mkdir \"" .. output_directory_hlsl .. "\"")
os.execute("if not exist \"" .. output_directory_c    .. "\" mkdir \"" .. output_directory_c    .. "\"")

local metashaders = {}

-- @PlatformSpecific
local metashader_dir = io.popen("dir \"" .. string.gsub(shader_directory, "/", "\\") .. "*.metashader\" /B")

for source in metashader_dir:lines() do
	local shader_path = shader_directory .. source

	local f        = io.open(shader_path, "r")
	local contents = f:read("*all")

	local result = parse_metashader_block(source, contents)
	
	if result == nil then
		error(source .. " does not have a @metashader block - that seems like a mistake, no codegen can happen")
	end

	table.insert(metashaders, {
		file_name = string.gsub(source, ".metashader", ""),
		path      = shader_path,
		bundle    = result,
	})

	f:close()
end

table.sort(metashaders, function(a, b)
	return a.file_name < b.file_name
end)

process_shaders({
	bundles               = metashaders, 
	output_directory_c    = output_directory_c, 
	output_directory_hlsl = output_directory_hlsl,
})

process_c_file("src/game/entities.h")
