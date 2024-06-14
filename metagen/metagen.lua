require "io"
require "os"
require "metagen/shader"

shader_directory      = "src/shaders/"
output_directory_hlsl = "src/shaders/gen/"
output_directory_c    = "src/game/render/shaders/gen/"

os.execute("if not exist \"" .. output_directory_hlsl .. "\" mkdir \"" .. output_directory_hlsl .. "\"")
os.execute("if not exist \"" .. output_directory_c    .. "\" mkdir \"" .. output_directory_c    .. "\"")

shaders = {
	"brush.dfs",
}

for _, shader_name in ipairs(shaders) do
	local shader_path = shader_directory .. shader_name

	local shader = dofile(shader_path)
	emit.emit_parameter_structs(shader, output_directory_c, output_directory_hlsl)
	emit.emit_shader_source(shader, output_directory_hlsl)
end
