require "io"
require "os"
require "metagen/shader"

shader_directory      = "src/shaders/"
output_directory_hlsl = "src/shaders/gen/"
output_directory_c    = "src/game/render/shaders/gen/"

-- @PlatformSpecific
os.execute("if not exist \"" .. output_directory_hlsl .. "\" mkdir \"" .. output_directory_hlsl .. "\"")
os.execute("if not exist \"" .. output_directory_c    .. "\" mkdir \"" .. output_directory_c    .. "\"")

local shader_sources = {}

-- @PlatformSpecific
local dir = io.popen("dir \"" .. string.gsub(shader_directory, "/", "\\") .. "*.dfs\" /B")

for source in dir:lines() do
	local name = string.gsub(source, ".dfs", "")
	print("Gathered shader source: " .. name)

	table.insert(shader_sources, name)
end

local shaders = {}

for _, name in ipairs(shader_sources) do
	local shader_path = shader_directory .. name .. ".dfs"
	local bundle = dofile(shader_path)

	if not bundle.disabled then
		assert(bundle, name .. ".dfs did not return a bundle")

		table.insert(shaders, {
			file_name = name,
			path      = shader_path,
			bundle    = bundle,
		})
	else
		io.stderr:write("Warning: Shader bundle '" .. bundle.name .. "' is disabled")
	end
end

-- TODO: Put this somewhere better
local view_parameters = {
	world_to_clip = float4x4,
	sun_matrix    = float4x4,
	sun_direction = float3,
	sun_color     = float3,
	view_size     = float2,
}

emit.process_shaders({
	bundles               = shaders, 
	view_parameters       = view_parameters,
	output_directory_c    = output_directory_c, 
	output_directory_hlsl = output_directory_hlsl,
})
