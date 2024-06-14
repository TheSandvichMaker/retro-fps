-- shader type definitions

uint   = { resource_type = "primitive", size = 1, align = 1, align_legacy = 1, c_name = "uint32_t", hlsl_name = "uint"   }
uint2  = { resource_type = "primitive", size = 2, align = 1, align_legacy = 2, c_name = "v2u_t",    hlsl_name = "uint2"  }
uint3  = { resource_type = "primitive", size = 3, align = 1, align_legacy = 3, c_name = "v3u_t",    hlsl_name = "uint3"  }
uint4  = { resource_type = "primitive", size = 4, align = 1, align_legacy = 4, c_name = "v4u_t",    hlsl_name = "uint4"  }
float  = { resource_type = "primitive", size = 1, align = 1, align_legacy = 1, c_name = "float",    hlsl_name = "float"  }
float2 = { resource_type = "primitive", size = 2, align = 1, align_legacy = 2, c_name = "v2_t",     hlsl_name = "float2" }
float3 = { resource_type = "primitive", size = 3, align = 1, align_legacy = 3, c_name = "v3_t",     hlsl_name = "float3" }
float4 = { resource_type = "primitive", size = 4, align = 1, align_legacy = 4, c_name = "v4_t",     hlsl_name = "float4" }

function StructuredBuffer(format)
	return {
		resource_type = "buffer",
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< StructuredBuffer< " .. format.hlsl_name .. " > >",
		c_name        = "rhi_buffer_srv_t",
	}
end

function Texture2D(format)
	format = format or "float4"

	return {
		resource_type = "texture",
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< Texture2D< " .. format.hlsl_name .. " > >",
		c_name        = "rhi_texture_srv_t",
	}
end

-- codegen

emit = {}

function emit.sort_parameters(in_parameters, cbuffer_kind)
	if cbuffer_kind == nil or (cbuffer_kind ~= "structured" and cbuffer_kind ~= "legacy") then 
		error("You need to specify a cbuffer kind (valid options: 'structured', 'legacy'")
	end

	local out_parameters = {}

	for k, v in pairs(in_parameters) do
		table.insert(out_parameters, { name = k, definition = v })
	end
	
	if cbuffer_kind == "structured" then
		table.sort(out_parameters, function(a, b) 
			if a.definition.align == b.definition.align then
				return a.name < b.name
			end

			return a.definition.align > b.definition.align
		end)
	elseif cbuffer_kind == "legacy" then
		table.sort(out_parameters, function(a, b) 
			if a.definition.align_legacy == b.definition.align_legacy then
				return a.name < b.name
			end

			return a.definition.align_legacy > b.definition.align_legacy
end)
	end

	return out_parameters
end

function emit.pop_next_parameter_for_align(parameters, wanted_align, cbuffer_kind)
	if cbuffer_kind == nil or (cbuffer_kind ~= "structured" and cbuffer_kind ~= "legacy") then 
		error("You need to specify a cbuffer kind (valid options: 'structured', 'legacy'")
	end

	local result = nil

	for k, v in ipairs(parameters) do
		local parameter_align = 0

		if cbuffer_kind == "structured" then
			parameter_align = v.definition.align
		elseif cbuffer_kind == "legacy" then
			parameter_align = v.definition.align_legacy
		else
			error("Invalid align")
		end

		if parameter_align <= wanted_align then
			table.remove(parameters, k)
			result = v
			break
		end
	end

	return result
end

function emit.create_packed_cbuffer(parameters, cbuffer_kind)
	if cbuffer_kind == nil or (cbuffer_kind ~= "structured" and cbuffer_kind ~= "legacy") then 
		error("You need to specify a cbuffer kind (valid options: 'structured', 'legacy'")
	end

	local result = {}
	local sorted = emit.sort_parameters(parameters, cbuffer_kind)
	local pad_index = 0

	while #sorted > 0 do
		local row_slots = 4
		
		while #sorted > 0 and row_slots > 0 do
			local parameter = emit.pop_next_parameter_for_align(sorted, row_slots, cbuffer_kind)

			if parameter ~= nil then
				table.insert(result, parameter)
				row_slots = row_slots - parameter.definition.size
				print("\tpacked '" .. parameter.name .. "' (type = '" .. parameter.definition.resource_type .. "', size = " .. parameter.definition.size .. ")")
			else
				local padding_type = nil
				if row_slots == 4 then padding_type = uint3 end
				if row_slots == 3 then padding_type = uint3 end
				if row_slots == 2 then padding_type = uint2 end
				if row_slots == 1 then padding_type = uint  end
				print("\tinserting padding for " .. row_slots .. " row slots")

				table.insert(result, { name = "pad" .. pad_index, definition = padding_type })
				row_slots = 0
				pad_index = pad_index + 1
			end
		end

		if cbuffer_kind == "legacy" then
			if row_slots > 0 then
				local padding_type = nil
				if row_slots == 4 then padding_type = uint3 end
				if row_slots == 3 then padding_type = uint3 end
				if row_slots == 2 then padding_type = uint2 end
				if row_slots == 1 then padding_type = uint  end
				print("\tinserting padding (trailing) for " .. row_slots .. " row slots")

				table.insert(result, { name = "pad" .. pad_index, definition = padding_type })
			end
		end
	end

	return result
end

function emit.c_emit_parameter_struct(struct_name, packed_parameters)
	print("Emitting parameter struct for C: " .. struct_name)

	io.write("typedef struct ", struct_name, "\n{\n")

	for _, v in ipairs(packed_parameters) do
		local name       = v.name
		local definition = v.definition

		if name == nil then
			error("parameter does not have a name")
		end

		if definition == nil then
			error("parameter '" .. name .. "' does not have a definition")
		end

		if definition.resource_type == nil then error("Invalid resource type for " .. name) end

		io.write("\t", definition.c_name, " ", name, ";\n")
	end

	io.write("} ", struct_name, ";\n\n")
end

function emit.hlsl_emit_parameter_struct(struct_name, packed_parameters)
	assert(struct_name)
	assert(packed_parameters)

	io.write("struct ", struct_name, "\n{\n")

	print("Emitting parameter struct for HLSL: " .. struct_name)

	for _, v in ipairs(packed_parameters) do
		local name       = v.name
		local definition = v.definition

		if name == nil then
			error("parameter does not have a name")
		end

		if definition == nil then
			error("parameter '" .. name .. "' does not have a definition")
		end

		if definition.resource_type == nil then error("Invalid resource type for " .. name) end

		io.write("\t", definition.hlsl_name, " ", name, ";\n")
	end

	io.write("};\n\n")
end

function emit.c_emit_parameter_set_function(shader_name, params_name, params, slot)
	io.write("void shader_" .. shader_name .. "_set_" .. slot .. "_params(rhi_command_list_t *list, " .. params_name .. " *params)\n{\n")
	io.write("\trhi_set_parameters(list, R1ParameterSlot_" .. slot .. ", params, sizeof(*params));\n")
	io.write("}\n\n")
end

function emit.emit_shader(shader, output_directory_c, output_directory_hlsl)
	local pass = nil
	local draw = nil

	print("Emitting shader parameters for '" .. shader.name .. "'")

	if shader.pass_parameters ~= nil then
		print("Packing pass parameters")
		pass = emit.create_packed_cbuffer(shader.pass_parameters, "legacy")
		if pass == nil then error("Failed to pack cbuffer") end
	end

	if shader.draw_parameters ~= nil then
		print("Packing draw parameters")
		draw = emit.create_packed_cbuffer(shader.draw_parameters, "structured")
		if draw == nil then error("Failed to pack cbuffer") end
	end

	local pass_struct_name = shader.name .. "_pass_parameters_t";
	local draw_struct_name = shader.name .. "_draw_parameters_t";

	-- emit c header

	io.output(output_directory_c .. shader.name .. ".h")
	io.write("// Generated by metagen.lua\n\n")
	io.write("#pragma once\n\n")

	if pass ~= nil then
		emit.c_emit_parameter_struct(pass_struct_name, pass)
	end

	if draw ~= nil then
		emit.c_emit_parameter_struct(draw_struct_name, draw)
	end

	if pass ~= nil then
		io.write("fn void shader_" .. shader.name .. "_set_pass_params(rhi_command_list_t *list, " .. pass_struct_name .. " *params);\n")
	end

	if draw ~= nil then
		io.write("fn void shader_" .. shader.name .. "_set_draw_params(rhi_command_list_t *list, " .. draw_struct_name .. " *params);\n")
	end

	-- emit c source

	io.output(output_directory_c .. shader.name .. ".c")
	io.write("// Generated by metagen.lua\n\n")

	if pass ~= nil then
		emit.c_emit_parameter_set_function(shader.name, pass_struct_name, shader.pass_parameters, "pass")
	end

	if draw ~= nil then
		emit.c_emit_parameter_set_function(shader.name, draw_struct_name, shader.draw_parameters, "draw")
	end

	-- emit hlsl

	io.output(output_directory_hlsl .. shader.name .. ".hlsl")
	io.write("// Generated by metagen.lua\n\n")
	io.write("#include \"bindless.hlsli\"\n\n")

	if pass ~= nil then
		emit.hlsl_emit_parameter_struct(pass_struct_name, pass)
		io.write("ConstantBuffer< " .. pass_struct_name .. " > pass : register(b1);\n\n")
	end

	if draw ~= nil then
		emit.hlsl_emit_parameter_struct(draw_struct_name, draw)
		io.write("ConstantBuffer< " .. draw_struct_name .. " > draw : register(b0);\n\n")
	end

	if shader.source then 
		io.write(shader.source)
	end
end

function emit.process_shaders(shaders, output_directory_c, output_directory_hlsl)
	-- emit shaders

	for _, shader_info in ipairs(shaders) do
		local shader_name = shader_info.name
		local shader      = shader_info.shader

		emit.emit_shader(shader, output_directory_c, output_directory_hlsl)
	end

	-- emit shaders.h

	local header = io.open(output_directory_c .. "shaders.h", "w")
	assert(header)

	header:write("// Generated by metagen.lua\n\n")
	header:write("#pragma once\n\n")

	for _, shader_info in ipairs(shaders) do
		header:write("#include \"" .. shader_info.name .. ".h\"\n")
	end

	header:write("\ntypedef enum df_shader_ident_t\n{\n")
	header:write("\tDfShader_none,\n\n")

	for _, shader_info in ipairs(shaders) do
		local shader = shader_info.shader
		local name   = shader_info.name
		local path   = shader_info.path

		for sub_name, sub_shader in pairs(shader.shaders) do
			header:write("\tDfShader_" .. name .. "_" .. sub_name .. ", // " .. path .. "\n")
		end

		header:write("\n")
	end

	header:write("\tDfShader_COUNT,\n")
	header:write("} df_shader_ident_t;\n\n")

	header:write("global df_shader_info_t df_shaders[DfShader_COUNT];\n")

	header:close()

	-- emit shaders.c

	local source = io.open(output_directory_c .. "shaders.c", "w")
	assert(source)

	source:write("// Generated by metagen.lua\n\n")

	for _, shader_info in ipairs(shaders) do
		local shader_name = shader_info.name
		local shader      = shader_info.shader

		emit.emit_shader(shader, output_directory_c, output_directory_hlsl)

		source:write("#include \"" .. shader_name .. ".c\"\n")
	end

	source:write("\n")

	source:write("df_shader_info_t df_shaders[DfShader_COUNT] = {\n")

	for _, shader_info in ipairs(shaders) do
		local shader = shader_info.shader
		local name   = shader_info.name
		local path   = shader_info.path

		for sub_name, sub_shader in pairs(shader.shaders) do
			source:write("\t[DfShader_" .. name .. "_" .. sub_name .. "] = {\n")
			source:write("\t\t.name        = Sc(\"" .. name .. "_" .. sub_name .. "\"),\n")
			source:write("\t\t.entry_point = Sc(\"" .. sub_shader.entry .. "\"),\n")
			source:write("\t\t.target      = Sc(\"" .. sub_shader.target .. "\"),\n")
			source:write("\t\t.path_hlsl   = Sc(\"" .. output_directory_hlsl .. shader.name .. ".hlsl" .. "\"),\n")
			source:write("\t\t.path_dfs    = Sc(\"" .. path .. "\"),\n")
			source:write("\t},\n")

			if next(shader.shaders, sub_name) ~= nil then
				source:write("\n")
			end
		end
	end

	source:write("};\n")

	source:close()
end
