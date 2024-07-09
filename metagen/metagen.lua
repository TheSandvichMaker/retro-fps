require "io"
require "os"
require "metagen/metashader"
require "metagen/emit"

shader_directory      = "src/shaders/"
output_directory_hlsl = "src/shaders/gen/"
output_directory_c    = "src/game/render/shaders/gen/"

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

-- TODO: Put this somewhere better
local view_parameters = {
	world_to_clip            = float4x4, -- also known as "view-projection matrix"
	view_to_clip             = float4x4, -- also known as "projection matrix"
	world_to_view            = float4x4, -- also known as "view matrix"
	sun_matrix               = float4x4,
	sun_direction            = float3,
	sun_color                = float3,
	view_size                = float2,
    fog_density              = float,
    fog_absorption           = float,
    fog_scattering           = float,
    fog_phase_k              = float,
    fog_ambient_inscattering = float3,
    frame_index              = uint,
}

process_shaders({
	bundles               = metashaders, 
	view_parameters       = view_parameters,
	output_directory_c    = output_directory_c, 
	output_directory_hlsl = output_directory_hlsl,
})
