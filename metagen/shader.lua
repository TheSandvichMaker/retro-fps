-- a little evil: make all unknown identifiers be treated as strings
-- so that I can make the syntax of metashader blocks look a bit nicer...
setmetatable(_G, {
	__index = function(table, key)
		return key
	end
})

shader_resource_mt = {}

local function shader_resource(t)
	setmetatable(t, shader_resource_mt)
	return t
end

function is_shader_resource_type(t)
	return getmetatable(t) == shader_resource_mt
end

-- shader type definitions

int      = shader_resource { resource_type = "primitive", size = 1,  align = 1, align_legacy = 1, c_name = "int32_t",  hlsl_name = "int"      }
uint     = shader_resource { resource_type = "primitive", size = 1,  align = 1, align_legacy = 1, c_name = "uint32_t", hlsl_name = "uint"     }
uint2    = shader_resource { resource_type = "primitive", size = 2,  align = 1, align_legacy = 2, c_name = "v2u_t",    hlsl_name = "uint2"    }
uint3    = shader_resource { resource_type = "primitive", size = 3,  align = 1, align_legacy = 3, c_name = "v3u_t",    hlsl_name = "uint3"    }
uint4    = shader_resource { resource_type = "primitive", size = 4,  align = 1, align_legacy = 4, c_name = "v4u_t",    hlsl_name = "uint4"    }
float    = shader_resource { resource_type = "primitive", size = 1,  align = 1, align_legacy = 1, c_name = "float",    hlsl_name = "float"    }
float2   = shader_resource { resource_type = "primitive", size = 2,  align = 1, align_legacy = 2, c_name = "v2_t",     hlsl_name = "float2"   }
float3   = shader_resource { resource_type = "primitive", size = 3,  align = 1, align_legacy = 3, c_name = "v3_t",     hlsl_name = "float3"   }
float4   = shader_resource { resource_type = "primitive", size = 4,  align = 1, align_legacy = 4, c_name = "v4_t",     hlsl_name = "float4"   }
float4x4 = shader_resource { resource_type = "primitive", size = 16, align = 1, align_legacy = 4, c_name = "m4x4_t",   hlsl_name = "float4x4" }

function StructuredBuffer(format)
	assert(format, "You need to pass a type to StructuredBuffer")

	local format_hlsl = nil

	if type(format) == "string" then
		format_hlsl = format
	else
		format_hlsl = format.hlsl_name
	end

	return shader_resource {
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

	return shader_resource {
		resource_type = "buffer",
		access        = "readwrite",
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< RWStructuredBuffer< " .. format_hlsl .. " > >",
		c_name        = "rhi_buffer_uav_t",
	}
end

function Texture2D(format, ex)
	local format_hlsl = "float4"

	if format then 
		assert(format.hlsl_name, "Invalid type passed to Texture2DMS")
		format_hlsl = format.hlsl_name
	end

	local may_be_null = false

	if ex ~= nil then
		if ex.may_be_null == true then
			may_be_null = true
		end
	end

	return shader_resource {
		resource_type = "texture",
		access        = "read",
		multisample   = false,
		may_be_null   = may_be_null,
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< Texture2D< " .. format_hlsl .. " > >",
		c_name        = "rhi_texture_srv_t",
	}
end

function Texture2DMS(format, ex)
	local format_hlsl = "float4"

	if format then 
		assert(format.hlsl_name, "Invalid type passed to Texture2DMS")
		format_hlsl = format.hlsl_name
	end

	local may_be_null = false

	if ex ~= nil then
		if ex.may_be_null == true then
			may_be_null = true
		end
	end

	return shader_resource {
		resource_type = "texture",
		access        = "read",
		multisample   = true,
		may_be_null   = may_be_null,
		size          = 1,
		align_legacy  = 4, -- NOTE: my resources are 16 byte aligned with legacy cb packing because they're structs, even though they should be able to be packed tightly because they're just indices...
		align         = 1,
		hlsl_name     = "df::Resource< Texture2DMS< " .. format_hlsl .. " > >",
		c_name        = "rhi_texture_srv_t",
	}
end

-- PSOs

function pso_fullscreen(ps, pf)
	return {
		vs = "fullscreen_triangle_vs",
		ps = ps,
		render_targets = {
			{ pf = pf },
		},
	}
end

function blend_alpha()
	return {
		blend_enable    = true,
		src_blend       = "src_alpha",
		dst_blend       = "inv_src_alpha",
		blend_op        = "add",
		src_blend_alpha = "inv_dest_alpha",
		dst_blend_alpha = "one",
		blend_op_alpha  = "add",
	}
end

function blend_premul_alpha()
	return {
		blend_enable    = true,
		src_blend       = "one",
		dst_blend       = "inv_src_alpha",
		blend_op        = "add",
		src_blend_alpha = "inv_dest_alpha",
		dst_blend_alpha = "one",
		blend_op_alpha  = "add",
	}
end

function blend_additive()
	return {
		blend_enable    = true,
		src_blend       = "one",
		dst_blend       = "one",
		blend_op        = "add",
		src_blend_alpha = "one",
		dst_blend_alpha = "one",
		blend_op_alpha  = "add",
	}
end