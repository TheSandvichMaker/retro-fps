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
	local shader = dofile(shader_path)

	table.insert(shaders, {
		name   = name,
		path   = shader_path,
		shader = shader,
	})
end

emit.process_shaders(shaders, output_directory_c, output_directory_hlsl)
