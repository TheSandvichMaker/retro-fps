-- error on access of undefined globals, to avoid stupid silent bugs

setmetatable(_G, {
	__index = function(table, key)
		error("Unknown global value: " .. key)
	end
})

-- shader type definitions

uint     = { resource_type = "primitive", size = 1,  align = 1, align_legacy = 1, c_name = "uint32_t", hlsl_name = "uint"     }
uint2    = { resource_type = "primitive", size = 2,  align = 1, align_legacy = 2, c_name = "v2u_t",    hlsl_name = "uint2"    }
uint3    = { resource_type = "primitive", size = 3,  align = 1, align_legacy = 3, c_name = "v3u_t",    hlsl_name = "uint3"    }
uint4    = { resource_type = "primitive", size = 4,  align = 1, align_legacy = 4, c_name = "v4u_t",    hlsl_name = "uint4"    }
float    = { resource_type = "primitive", size = 1,  align = 1, align_legacy = 1, c_name = "float",    hlsl_name = "float"    }
float2   = { resource_type = "primitive", size = 2,  align = 1, align_legacy = 2, c_name = "v2_t",     hlsl_name = "float2"   }
float3   = { resource_type = "primitive", size = 3,  align = 1, align_legacy = 3, c_name = "v3_t",     hlsl_name = "float3"   }
float4   = { resource_type = "primitive", size = 4,  align = 1, align_legacy = 4, c_name = "v4_t",     hlsl_name = "float4"   }
float4x4 = { resource_type = "primitive", size = 16, align = 1, align_legacy = 4, c_name = "m4x4_t",   hlsl_name = "float4x4" }

function StructuredBuffer(format)
	assert(format, "You need to pass a type to StructuredBuffer")

	local format_hlsl = nil

	if type(format) == "string" then
		format_hlsl = format
	else
		format_hlsl = format.hlsl_name
	end

	return {
		resource_type = "buffer",
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< StructuredBuffer< " .. format_hlsl .. " > >",
		c_name        = "rhi_buffer_srv_t",
	}
end

function Texture2D(format)
	local format_hlsl = "float4"

	if format then 
		assert(format.hlsl_name, "Invalid type passed to Texture2DMS")
		format_hlsl = format.hlsl_name
	end

	return {
		resource_type = "texture",
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< Texture2D< " .. format.hlsl_name .. " > >",
		c_name        = "rhi_texture_srv_t",
	}
end

function Texture2DMS(format)
	local format_hlsl = "float4"

	if format then 
		assert(format.hlsl_name, "Invalid type passed to Texture2DMS")
		format_hlsl = format.hlsl_name
	end

	return {
		resource_type = "texture",
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< Texture2DMS< " .. format_hlsl .. " > >",
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
		if v == nil then
			error("Parameter defined with unknown type: " .. name)
		end
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
	end

	return result
end

function emit.c_emit_parameter_struct(struct_name, packed_parameters)
	print("Emitting parameter struct for C: " .. struct_name)

	io.write("typedef struct ", struct_name, "\n{\n")

	for _, v in ipairs(packed_parameters) do
		local name       = v.name
		local definition = v.definition

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

		io.write("\t", definition.hlsl_name, " ", name, ";\n")
	end

	io.write("};\n\n")
end

function emit.c_emit_parameter_set_function(function_name, params_name, slot)
	io.write("void " .. function_name .. "(rhi_command_list_t *list, " .. params_name .. " *params)\n{\n")
	io.write("\trhi_set_parameters(list, " .. slot .. ", params, sizeof(*params));\n")
	io.write("}\n\n")
end

function emit.emit_bundle(bundle_info, output_directory_c, output_directory_hlsl)
	local bundle           = bundle_info.bundle
	local bundle_file_name = bundle_info.name

	local pass = nil
	local draw = nil

	if not bundle.name then
		error(bundle_file_name .. ".dfs did not provide a 'name' field")
	end

	assert(bundle.name, "You have to supply a bundle name")
	print("Emitting shader parameters for '" .. bundle.name .. "'")

	-- gather and pack parameter structs

	local parameter_structs = {
		{ name = "draw", slot = 0, params = nil, cbuffer = nil, type_name = nil },
		{ name = "pass", slot = 1, params = nil, cbuffer = nil, type_name = nil },
	}

	for i, v in ipairs(parameter_structs) do
		if bundle[v.name .. "_parameters"] then
			v.params    = bundle[v.name .. "_parameters"]
			v.cbuffer   = emit.create_packed_cbuffer(v.params, "legacy")
			v.type_name = bundle.name .. "_" .. v.name .. "_parameters_t"
			assert(v.cbuffer)
		end
	end

	-- emit c header

	io.output(output_directory_c .. bundle.name .. ".h")
	io.write("// Generated by metagen.lua\n\n")
	io.write("#pragma once\n\n")

	for i, v in ipairs(parameter_structs) do
		if v.cbuffer then
			emit.c_emit_parameter_struct(v.type_name, v.cbuffer)
			io.write("fn void shader_" .. bundle.name .. "_set_" .. v.name .. "_params(rhi_command_list_t *list, " .. v.type_name .. " *params);\n")
		end
	end

	-- emit c source

	io.output(output_directory_c .. bundle.name .. ".c")
	io.write("// Generated by metagen.lua\n\n")

	for i, v in ipairs(parameter_structs) do
		if v.cbuffer then
			local function_name = "shader_" .. bundle.name .. "_set_" .. v.name .. "_params"
			emit.c_emit_parameter_set_function(function_name, v.type_name, v.slot)
		end
	end

	-- emit hlsl

	io.output(output_directory_hlsl .. bundle.name .. ".hlsl")
	io.write("// Generated by metagen.lua\n\n")
	io.write("#include \"bindless.hlsli\"\n\n")

	if bundle.prelude then
		io.write(bundle.prelude)
	end

	for i, v in ipairs(parameter_structs) do
		if v.cbuffer then
			emit.hlsl_emit_parameter_struct(v.type_name, v.cbuffer)
			io.write("ConstantBuffer< " .. v.type_name .. " > " .. v.name .. " : register(b" .. v.slot .. ");\n\n")
		end
	end

	-- later, this may not be an error
	assert(bundle.source, bundle.name .. ".dfs did not provide any shader source code. Make sure to define the 'source' field in your bundle.")

	if bundle.source then 
		io.write(bundle.source)
	end
end

function emit.process_shaders(p)
	local bundles               = p.bundles
	local view_parameters       = p.view_parameters
	local output_directory_c    = p.output_directory_c
	local output_directory_hlsl = p.output_directory_hlsl

	-- emit shaders

	for _, bundle_info in ipairs(bundles) do
		emit.emit_bundle(bundle_info, output_directory_c, output_directory_hlsl)
	end

	-- emit view buffer

	local view_cbuffer = emit.create_packed_cbuffer(view_parameters, "legacy")

	io.output(output_directory_c .. "view.h")
	io.write("// Generated by metagen.lua\n\n")
	io.write("#pragma once\n\n")

	emit.c_emit_parameter_struct("view_parameters_t", view_cbuffer)
	io.write("fn void set_view_parameters(rhi_command_list_t *list, view_parameters_t *params);\n")

	io.output(output_directory_c .. "view.c")
	io.write("// Generated by metagen.lua\n\n")

	emit.c_emit_parameter_set_function("set_view_parameters", "view_parameters_t", 2)

	io.output(output_directory_hlsl .. "view.hlsli")
	io.write("// Generated by metagen.lua\n\n")

	emit.hlsl_emit_parameter_struct("view_parameters_t", view_cbuffer)
	io.write("ConstantBuffer< view_parameters_t > view : register(b2);\n")

	-- emit shaders.h

	local header = io.open(output_directory_c .. "shaders.h", "w")
	assert(header)

	header:write("// Generated by metagen.lua\n\n")
	header:write("#pragma once\n\n")

	header:write("#include \"view.h\"\n")

	for _, bundle_info in ipairs(bundles) do
		header:write("#include \"" .. bundle_info.name .. ".h\"\n")
	end

	header:write("\ntypedef enum df_shader_ident_t\n{\n")
	header:write("\tDfShader_none,\n\n")

	for _, bundle_info in ipairs(bundles) do
		local bundle      = bundle_info.bundle
		local bundle_name = bundle_info.name
		local bundle_path = bundle_info.path

		-- maybe shouldn't be an error, but for now it is
		assert(bundle.shaders, bundle_name .. ".dfs did not export any shaders")

		if bundle.shaders then
			for shader_name, shader in pairs(bundle.shaders) do
				header:write("\tDfShader_" .. bundle_name .. "_" .. shader_name .. ", // " .. bundle_path .. "\n")
			end
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

	source:write("#include \"view.c\"\n")

	for _, bundle_info in ipairs(bundles) do
		local bundle_name = bundle_info.name
		source:write("#include \"" .. bundle_name .. ".c\"\n")
	end

	source:write("\n")

	source:write("df_shader_info_t df_shaders[DfShader_COUNT] = {\n")

	for _, bundle_info in ipairs(bundles) do
		local bundle      = bundle_info.bundle
		local bundle_name = bundle_info.name
		local bundle_path = bundle_info.path

		if bundle.shaders then
			for shader_name, shader in pairs(bundle.shaders) do
				source:write("\t[DfShader_" .. bundle_name .. "_" .. shader_name .. "] = {\n")
				source:write("\t\t.name        = Sc(\"" .. bundle_name .. "_" .. shader_name .. "\"),\n")
				source:write("\t\t.entry_point = Sc(\"" .. shader.entry .. "\"),\n")
				source:write("\t\t.target      = Sc(\"" .. shader.target .. "\"),\n")
				source:write("\t\t.path_hlsl   = Sc(\"" .. output_directory_hlsl .. bundle.name .. ".hlsl" .. "\"),\n")
				source:write("\t\t.path_dfs    = Sc(\"" .. bundle_path .. "\"),\n")
				source:write("\t},\n")

				if next(bundle.shaders, shader_name) ~= nil then
					source:write("\n")
				end
			end
		end
	end

	source:write("};\n")

	source:close()
end
