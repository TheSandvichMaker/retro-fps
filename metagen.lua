require "io"
require "shader"

print("running metagen.lua")

shader_directory = "src/shaders/"

shaders = {
	"brush.hlsl",
	"debug_lines.hlsl",
	"post.hlsl",
	"shadow.hlsl",
	"triangle.hlsl",
	"ui.hlsl",
}

brush = require "draft_shader"

io.output("shader_brush.h")
emit.c_parameter_struct(brush.name .. "_draw_parameters_t", brush.draw_parameters)
emit.c_parameter_struct(brush.name .. "_pass_parameters_t", brush.pass_parameters)
