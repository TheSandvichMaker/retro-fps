float  = { resource_type = "primitive", size = 1, c_name = "float", hlsl_name = "float"  }
float2 = { resource_type = "primitive", size = 2, c_name = "v2_t",  hlsl_name = "float2" }
float3 = { resource_type = "primitive", size = 3, c_name = "v3_t",  hlsl_name = "float3" }
float4 = { resource_type = "primitive", size = 4, c_name = "v4_t",  hlsl_name = "float4" }

function StructuredBuffer(format)
	return {
		resource_type = "buffer",
		size          = 1,
		hlsl_name     = "df::Resource< StructuredBuffer< " .. format.hlsl_name .. " > >",
		c_name        = "rhi_buffer_srv_t",
	}
end

function Texture2D(format)
	format = format or "float4"

	return {
		resource_type = "texture",
		size          = 1,
		hlsl_name     = "df::Resource< Texture2D< " .. format.hlsl_name .. " > >",
		c_name        = "rhi_texture_srv_t",
	}
end

emit = {}

function dump(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
         if type(k) ~= 'number' then k = '"'..k..'"' end
         s = s .. '['..k..'] = ' .. dump(v) .. ','
      end
      return s .. '} '
   else
      return tostring(o)
   end
end

function emit.sort_parameters(in_parameters)
	local out_parameters = {}

	for k, v in pairs(in_parameters) do
		table.insert(out_parameters, { name = k, definition = v })
	end
	
	table.sort(out_parameters, function(a, b) 
		if a.definition.size == b.definition.size then
			return a.name < b.name
		end

		return a.definition.size > b.definition.size
	end)

	return out_parameters
end

function emit.c_parameter_struct(struct_name, parameters)
	io.write("typedef struct ", struct_name, "\n{\n")

	local sorted = emit.sort_parameters(parameters)

	for _, v in ipairs(sorted) do
		local name       = v.name
		local definition = v.definition

		if definition.resource_type == nil then error("Invalid resource type for " .. name) end

		io.write("\talignas(16) ", definition.c_name, " ", name, ";\n")
	end

	io.write("} ", struct_name, ";\n\n")
end
