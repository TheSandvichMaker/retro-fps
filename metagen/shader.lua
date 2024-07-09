-- error on access of undefined globals, to avoid stupid silent bugs

setmetatable(_G, {
	__index = function(table, key)
		error("Unknown global value: " .. key)
	end
})

-- shader type definitions

int      = { resource_type = "primitive", size = 1,  align = 1, align_legacy = 1, c_name = "int32_t", hlsl_name = "int"     }
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
		access        = "read",
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< StructuredBuffer< " .. format_hlsl .. " > >",
		c_name        = "rhi_buffer_srv_t",
	}
end

function RWStructuredBuffer(format)
	assert(format, "You need to pass a type to RWStructuredBuffer")

	local format_hlsl = nil

	if type(format) == "string" then
		format_hlsl = format
	else
		format_hlsl = format.hlsl_name
	end

	return {
		resource_type = "buffer",
		access        = "readwrite",
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< RWStructuredBuffer< " .. format_hlsl .. " > >",
		c_name        = "rhi_buffer_uav_t",
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
		access        = "read",
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< Texture2D< " .. format_hlsl .. " > >",
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
		access        = "read",
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< Texture2DMS< " .. format_hlsl .. " > >",
		c_name        = "rhi_texture_srv_t",
	}
end

