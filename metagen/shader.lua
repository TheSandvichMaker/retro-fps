uint   = { resource_type = "primitive", size = 1, align_legacy = 1, align_root = 1, c_name = "uint32_t", hlsl_name = "uint"   }
uint2  = { resource_type = "primitive", size = 2, align_legacy = 2, align_root = 2, c_name = "v2u_t",    hlsl_name = "uint2"  }
uint3  = { resource_type = "primitive", size = 3, align_legacy = 3, align_root = 3, c_name = "v3u_t",    hlsl_name = "uint3"  }
uint4  = { resource_type = "primitive", size = 4, align_legacy = 4, align_root = 4, c_name = "v4u_t",    hlsl_name = "uint4"  }
float  = { resource_type = "primitive", size = 1, align_legacy = 1, align_root = 1, c_name = "float",    hlsl_name = "float"  }
float2 = { resource_type = "primitive", size = 2, align_legacy = 2, align_root = 2, c_name = "v2_t",     hlsl_name = "float2" }
float3 = { resource_type = "primitive", size = 3, align_legacy = 3, align_root = 3, c_name = "v3_t",     hlsl_name = "float3" }
float4 = { resource_type = "primitive", size = 4, align_legacy = 4, align_root = 4, c_name = "v4_t",     hlsl_name = "float4" }

function StructuredBuffer(format)
	return {
		resource_type = "buffer",
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align_root    = 1,
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
		align_root    = 1,
		hlsl_name     = "df::Resource< Texture2D< " .. format.hlsl_name .. " > >",
		c_name        = "rhi_texture_srv_t",
	}
end

emit = {}

function emit.sort_parameters(in_parameters)
	local out_parameters = {}

	for k, v in pairs(in_parameters) do
		table.insert(out_parameters, { name = k, definition = v })
	end
	
	table.sort(out_parameters, function(a, b) 
		if a.definition.align == b.definition.align then
			return a.name < b.name
		end

		return a.definition.align > b.definition.align
	end)

	return out_parameters
end

function emit.pop_next_parameter_for_align(parameters, wanted_align, cbuffer_kind)
	if cbuffer_kind == nil or (cbuffer_kind ~= "structured" and cbuffer_kind ~= "legacy") then 
		error("You need to specify a cbuffer kind (valid options: 'root', 'legacy'")
	end

	local result = nil

	for k, v in ipairs(parameters) do
		local parameter_align = 0

		if cbuffer_kind == "structured" then
			parameter_align = v.definition.align_root
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
		error("You need to specify a cbuffer kind (valid options: 'root', 'legacy'")
	end

	local result = {}
	local sorted = emit.sort_parameters(parameters)
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

function emit.emit_parameter_structs(shader, output_directory_c, output_directory_hlsl)
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

	-- emit C

	io.output(output_directory_c .. shader.name .. ".gen.h")
	io.write("#pragma once\n\n")

	if pass ~= nil then
		emit.c_emit_parameter_struct(shader.name .. "_pass_parameters_t", pass)
	end

	if draw ~= nil then
		emit.c_emit_parameter_struct(shader.name .. "_draw_parameters_t", draw)
	end

	-- emit HLSL

	io.output(output_directory_hlsl .. shader.name .. ".gen.hlsli")
	io.write("#pragma once\n\n")

	if pass ~= nil then
		emit.hlsl_emit_parameter_struct(shader.name .. "_pass_parameters_t", pass)
	end

	if draw ~= nil then
		emit.hlsl_emit_parameter_struct(shader.name .. "_draw_parameters_t", draw)
	end
end

function emit.emit_shader_source(shader, output_directory)
	if shader.source == nil then error("error: shader '" .. shader.name .. "' has no source member!") end
	io.output(output_directory .. shader.name .. ".hlsl")
	io.write(shader.source)
end