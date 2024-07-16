require "metagen/shader"

local cbuf_packing_mode = "legacy"

-- codegen

emit = {}

local function error_context()
	if emit.current_bundle_info ~= nil then
		local bundle_file_name = emit.current_bundle_info.file_name
		return "[" .. bundle_file_name .. ".metashader]: "
	end

	return ""
end

function emit.error(str)
	error("\n" .. error_context() .. str)
end

function emit.sort_parameters(in_parameters, cbuffer_kind)
	if cbuffer_kind == nil or (cbuffer_kind ~= "structured" and cbuffer_kind ~= "legacy") then 
		error("You need to specify a cbuffer kind (valid options: 'structured', 'legacy'")
	end

	local out_parameters = {}

	for k, v in pairs(in_parameters) do
		if not is_shader_resource_type(v) then
			emit.error("The type of parameter '" .. k .. "': '" .. tostring(v) .. "' is not recognized as a valid shader input type")
		end

		table.insert(out_parameters, { name = k, definition = v })
		if v == nil then
			emit.error("Parameter defined with unknown type: " .. name)
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
		emit.error("You need to specify a cbuffer kind (valid options: 'structured', 'legacy'")
	end

	local result = nil

	for k, v in ipairs(parameters) do
		local parameter_align = 0

		if cbuffer_kind == "structured" then
			parameter_align = v.definition.align
		elseif cbuffer_kind == "legacy" then
			parameter_align = v.definition.align_legacy
		else
			emit.error("Invalid align")
		end

		if parameter_align <= wanted_align then
			table.remove(parameters, k)
			result = v
			break
		end
	end

	return result
end

function emit.create_packed_cbuffer(parameters, cbuffer_kind, name)
	if cbuffer_kind == nil or (cbuffer_kind ~= "structured" and cbuffer_kind ~= "legacy") then 
		emit.error("You need to specify a cbuffer kind (valid options: 'structured', 'legacy'")
	end

	local result = { name = name }
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

function emit.c_emit_parameter_struct(struct_name, packed_parameters, is_draw_parameters)
	print("Emitting parameter struct for C: " .. struct_name)

	io.write("typedef struct ", struct_name, "\n{\n")

	for _, v in ipairs(packed_parameters) do
		local name       = v.name
		local definition = v.definition

		io.write("\t", definition.c_name, " ", name, ";\n")
	end

	io.write("} ", struct_name, ";\n\n")

	if is_draw_parameters == true then
		io.write("static_assert(sizeof(" .. struct_name .. ") <= sizeof(uint32_t)*60, \"Draw parameters (which are passed as root constants) can't be larger than 60 uint32s\");\n\n")
	end
end

function emit.table_to_sorted_array(the_table)
	local result = {}

	for k, v in pairs(the_table) do
		table.insert(result, { k = k, v = v })
	end

	table.sort(result, function(a, b) return a.k < b.k end)

	return result
end

-- returns a string, does not emit directly
function emit.hlsl_emit_parameter_struct(struct_name, packed_parameters)
	assert(struct_name)
	assert(packed_parameters)

	print("Emitting parameter struct for HLSL: " .. struct_name)

	local result = ""

	result = result .. "struct " .. struct_name .. "\n{\n";

	for _, v in ipairs(packed_parameters) do
		local name       = v.name
		local definition = v.definition

		result = result .. "\t" .. definition.hlsl_name .. " " .. name .. ";\n"
	end

	result = result .. "};\n\n"

	return result
end

local function add_flag(flags, flag, condition)
	local result = flags

	if condition == true then
		if flags == "0" then
			result = flag
		else
			result = flags .. "|" .. flag
		end
	end

	return result
end

function emit.c_emit_parameter_set_function(function_name, params_name, cbuffer)
	io.write("void " .. function_name .. "(rhi_command_list_t *list, uint32_t slot, " .. params_name .. " *params)\n{\n")

	for i, v in ipairs(cbuffer) do
		local name       = v.name
		local definition = v.definition

		if definition.resource_type == "buffer" then
			if definition.access == "read" then
				io.write("\trhi_validate_buffer_srv(params->" .. name .. ", S(\"" .. params_name .. "::" .. name .. "\"));\n")
			end
		elseif definition.resource_type == "texture" then
			local flags = "0";

			flags = add_flag(flags, "RhiValidateTextureSrv_is_msaa", definition.multisample)
			flags = add_flag(flags, "RhiValidateTextureSrv_may_be_null", definition.may_be_null)

			io.write("\trhi_validate_texture_srv(params->" .. name .. ", S(\"" .. params_name .. "::" .. name .. "\"), " .. flags .. ");\n")
		end
	end

	io.write("\trhi_set_parameters(list, slot, params, sizeof(*params));\n")
	io.write("}\n\n")
end

-- local view_parameters_omega_cheat = nil

function emit.emit_bundle(context, bundle_info, output_directory_c, output_directory_hlsl)
	emit.current_bundle_info = bundle_info

	local bundle           = bundle_info.bundle
	local bundle_file_name = bundle_info.file_name

	-- FIXME: this is a hack
	bundle.name = bundle_file_name

	print("Emitting shader parameters (v2) for '" .. bundle.name .. "'")

	-- process exports

	local draw_params_name    = bundle.draw_parameters
	local draw_params_cbuffer = nil

	local pass_params_name    = bundle.pass_parameters
	local pass_params_cbuffer = nil

	local view_params_name    = bundle.view_parameters
	local view_params_cbuffer = nil

	local cbuffers = {}

	if bundle.exports then
		for export_name, export in pairs(bundle.exports) do
			if getmetatable(export) == cbuffer_mt then
				-- emit cbuffer

				local cbuffer = emit.create_packed_cbuffer(export, "legacy", export_name)
				assert(cbuffer)

				table.insert(cbuffers, cbuffer)

				if export_name == draw_params_name then
					if draw_params_cbuffer ~= nil then
						emit.error("Error: Two different exported cbuffers have the same name")
					end

					draw_params_cbuffer = cbuffer
				end

				if export_name == pass_params_name then
					if pass_params_cbuffer ~= nil then
						emit.error("Error: Two different exported cbuffers have the same name")
					end

					pass_params_cbuffer = cbuffer
				end

				if export_name == view_params_name then
					if view_params_cbuffer ~= nil then
						emit.error("Error: Two different exported cbuffers have the same name")
					end

					view_params_cbuffer = cbuffer
				end
			end

			if getmetatable(export) == flags_mt then
				-- emit flags
				emit.error("TODO: Implement flags")
			end
		end
	end

	-- emit c header

	io.output(output_directory_c .. bundle.name .. ".h")
	io.write("// Generated by metagen.lua\n\n")
	io.write("#pragma once\n\n")

	for i, cbuffer in ipairs(cbuffers) do
		emit.c_emit_parameter_struct(cbuffer.name, cbuffer, cbuffer == draw_params_cbuffer)
		io.write("fn void shader_set_params__" .. cbuffer.name .. "(rhi_command_list_t *list, uint32_t slot, " .. cbuffer.name .. " *parameters);")
	end

	io.write("global string_t " .. bundle.name .. "_source_code;\n")

	-- emit hlsli

	local has_hlsli  = #cbuffers > 0
	local hlsli_name = bundle.name .. ".hlsli"

	if has_hlsli then
		io.output(output_directory_hlsl .. hlsli_name)
		io.write("// Generated by metagen.lua\n\n#include \"bindless.hlsli\"\n\n")

		for i, cbuffer in ipairs(cbuffers) do
			io.write(emit.hlsl_emit_parameter_struct(cbuffer.name, cbuffer))
		end
	end

	-- emit hlsl

	local hlsl_source = "// Generated by metagen.lua\n\n"

	if bundle.prelude then
		hlsl_source = hlsl_source .. bundle.prelude
	end

	if has_hlsli then
		-- TODO: Fix this include to not have "gen" in it hardcoded
		hlsl_source = hlsl_source .. "#include \"gen/" .. hlsli_name .. "\"\n\n"
	end

	if draw_params_cbuffer ~= nil then
		hlsl_source = hlsl_source .. "ConstantBuffer< " .. draw_params_cbuffer.name .. " > draw : register(b0);"
	end

	if pass_params_cbuffer ~= nil then
		hlsl_source = hlsl_source .. "ConstantBuffer< " .. pass_params_cbuffer.name .. " > pass : register(b1);"
	end

	if view_params_cbuffer ~= nil then
		hlsl_source = hlsl_source .. "ConstantBuffer< " .. view_params_cbuffer.name .. " > view : register(b2);"
	end

	if bundle.source then 
		hlsl_source = hlsl_source .. bundle.source
	end

	io.output(output_directory_hlsl .. bundle.name .. ".hlsl")
	io.write(hlsl_source)

	-- emit c source

	io.output(output_directory_c .. bundle.name .. ".c")
	io.write("// Generated by metagen.lua\n\n")

	for i, cbuffer in ipairs(cbuffers) do
		emit.c_emit_parameter_set_function("shader_set_params__" .. cbuffer.name, cbuffer.name, cbuffer)
	end

	io.write("#define DF_SHADER_" .. string.upper(bundle.name) .. "_SOURCE_CODE \\\n")

	for line in string.gmatch(hlsl_source, "([^\n]*)\n?") do
		local escaped = string.gsub(line, "\"", "\\\"")
		io.write("\t\"" .. escaped .. "\\n\" \\\n")
	end

	-- gather cbuffers

	if context.cbuffers == nil then
		context.cbuffers = {}
	end

	local context_cbuffers = context.cbuffers;

	for i = 1, #cbuffers do
		context_cbuffers[#context_cbuffers + 1] = cbuffers[i]
	end
end

local function process_shaders_internal(p)
	local bundles               = p.bundles
	-- local view_parameters       = p.view_parameters
	local output_directory_c    = p.output_directory_c
	local output_directory_hlsl = p.output_directory_hlsl

	-- emit shaders

	local context = {}
	context.cbuffers = {}

	for _, bundle_info in ipairs(bundles) do
		emit.emit_bundle(context, bundle_info, output_directory_c, output_directory_hlsl)
	end

	-- emit shaders.h

	-- shaders

	local header = io.open(output_directory_c .. "shaders.h", "w")
	assert(header)

	header:write("// Generated by metagen.lua\n\n")
	header:write("#pragma once\n\n")

	for _, bundle_info in ipairs(bundles) do
		header:write("#include \"" .. bundle_info.bundle.name .. ".h\"\n")
	end

	header:write("\ntypedef enum df_shader_ident_t\n{\n")
	header:write("\tDfShader_none,\n\n")

	for _, bundle_info in ipairs(bundles) do
		local bundle      = bundle_info.bundle
		local bundle_name = bundle.name
		local bundle_path = bundle_info.path

		if bundle.shaders then
			local shaders = emit.table_to_sorted_array(bundle.shaders)

			header:write("\t// " .. bundle_path .. "\n")
			for i, v in ipairs(shaders) do
				local shader_name = v.k
				local shader      = v.v
				header:write("\tDfShader_" .. shader_name .. ",\n")
			end
		end

		header:write("\n")
	end

	header:write("\tDfShader_COUNT,\n")
	header:write("} df_shader_ident_t;\n\n")

	header:write("global df_shader_info_t df_shaders[DfShader_COUNT];\n\n")

	-- PSOs

	header:write("typedef enum df_pso_ident_t\n{\n\tDfPso_none,\n\n")

	for _, bundle_info in ipairs(bundles) do
		local bundle      = bundle_info.bundle
		local bundle_name = bundle.name
		local bundle_path = bundle_info.path

		if bundle.psos then
			local psos = emit.table_to_sorted_array(bundle.psos)

			header:write("\t// " .. bundle_path .. "\n")
			for i, v in ipairs(psos) do
				local pso_name = v.k
				local pso      = v.v
				header:write("\tDfPso_" .. pso_name .. ",\n")
			end

			header:write("\n")
		end
	end

	header:write("\tDfPso_COUNT,\n")
	header:write("} df_pso_ident_t;\n\n")

	header:write("global df_pso_info_t df_psos[DfPso_COUNT];\n\n")

	-- cbuffer set function macros

	if #context.cbuffers > 0 then
		header:write("#define shader_set_params(list, slot, params) \\\n")
		header:write("\t_Generic(*(params), \\\n")

		for i = 1, #context.cbuffers do
			local cbuffer = context.cbuffers[i]

			if i == #context.cbuffers then
				header:write("\t\t" .. cbuffer.name .. ": shader_set_params__" .. cbuffer.name .. "  \\\n")
			else
				header:write("\t\t" .. cbuffer.name .. ": shader_set_params__" .. cbuffer.name .. ", \\\n")
			end
		end

		header:write("\t)(list, slot, params)\n\n")

		-- header:write("#define shader_set_draw_params(list, params) shader_set_params(list, 0, params)\n")
		-- header:write("#define shader_set_pass_params(list, params) shader_set_params(list, 1, params)\n")
		-- header:write("#define shader_set_view_params(list, params) shader_set_params(list, 2, params)\n\n")
	end

	header:close()

	-- emit shaders.c

	local source = io.open(output_directory_c .. "shaders.c", "w")
	assert(source)

	source:write("// Generated by metagen.lua\n\n")

	for _, bundle_info in ipairs(bundles) do
		local bundle_name = bundle_info.bundle.name
		source:write("#include \"" .. bundle_name .. ".c\"\n")
	end

	source:write("\n")

	source:write("df_shader_info_t df_shaders[DfShader_COUNT] = {\n")

	for i, bundle_info in ipairs(bundles) do
		local bundle      = bundle_info.bundle
		local bundle_name = bundle_info.bundle.name
		local bundle_path = bundle_info.path

		if bundle.shaders then
			local shaders = emit.table_to_sorted_array(bundle.shaders)

			for j, v in ipairs(shaders) do
				local shader_name  = v.k
				local shader       = v.v
				local shader_entry = shader.entry or shader_name

				source:write("\t[DfShader_" .. shader_name .. "] = {\n")
				source:write("\t\t.name        = Sc(\"" .. shader_name .. "\"),\n")
				source:write("\t\t.entry_point = Sc(\"" .. shader_entry .. "\"),\n")
				source:write("\t\t.target      = Sc(\"" .. shader.target .. "\"),\n")
				source:write("\t\t.hlsl_source = Sc(DF_SHADER_" .. bundle.name:upper() .. "_SOURCE_CODE),\n")
				source:write("\t\t.path_hlsl   = Sc(\"" .. output_directory_hlsl .. bundle.name .. ".hlsl" .. "\"),\n")
				source:write("\t\t.path_dfs    = Sc(\"" .. bundle_path .. "\"),\n")
				source:write("\t},\n")

				if next(shaders, j) == nil and 
				   next(bundles, i) ~= nil then
					source:write("\n")
				end
			end
		end
	end

	source:write("};\n\n")

	-- PSOs

	source:write("df_pso_info_t df_psos[DfPso_COUNT] = {\n")

	for i, bundle_info in ipairs(bundles) do
		local bundle      = bundle_info.bundle
		local bundle_name = bundle_info.bundle.name
		local bundle_path = bundle_info.path

		if bundle.psos then
			local psos = emit.table_to_sorted_array(bundle.psos)

			for j, v in ipairs(psos) do
				local pso_name = v.k
				local pso      = v.v
				source:write("\t[DfPso_" .. pso_name .. "] = {\n")
				source:write("\t\t.name = Sc(\"" .. pso_name .. "\"),\n")
				source:write("\t\t.path = Sc(\"" .. bundle_path .. "\"),\n")
				if pso.vs ~= nil then source:write("\t\t.vs_index = DfShader_" .. pso.vs .. ",\n") end
				if pso.ps ~= nil then source:write("\t\t.ps_index = DfShader_" .. pso.ps .. ",\n") end
				source:write("\t\t.params_without_bytecode = {\n")

				-- params
				source:write("\t\t\t.debug_name = Sc(\"pso_" .. pso_name ..  "\"),\n")

				-- blend
				if pso.alpha_to_coverage == true then
					source:write("\t\t\t.blend.alpha_to_coverage_enable = true,\n")
				end

				if pso.independent_blend == true then
					source:write("\t\t\t.blend.independent_blend_enable = true,\n")
				end

				if pso.sample_mask ~= nil then
					source:write("\t\t\t.blend.sample_mask = " .. pso.sample_mask .. ",\n")
				else
					source:write("\t\t\t.blend.sample_mask = 0xFFFFFFFF,\n")
				end

				if pso.render_targets ~= nil then
					local function emit_rt_blend(desc)
						if desc.blend_enable == true then
							source:write("\t\t\t\t.blend_enable = true,\n")
						end

						if desc.logic_op_enable == true then
							source:write("\t\t\t\t.logic_op_enable = true,\n")
						end

						if desc.src_blend ~= nil then
							source:write("\t\t\t\t.src_blend = RhiBlend_" .. desc.src_blend .. ",\n")
						end

						if desc.dst_blend ~= nil then
							source:write("\t\t\t\t.dst_blend = RhiBlend_" .. desc.dst_blend .. ",\n")
						end

						if desc.blend_op ~= nil then
							source:write("\t\t\t\t.blend_op = RhiBlendOp_" .. desc.blend_op .. ",\n")
						end

						if desc.src_blend_alpha ~= nil then
							source:write("\t\t\t\t.src_blend_alpha = RhiBlend_" .. desc.src_blend_alpha .. ",\n")
						end

						if desc.dst_blend_alpha ~= nil then
							source:write("\t\t\t\t.dst_blend_alpha = RhiBlend_" .. desc.dst_blend_alpha .. ",\n")
						end

						if desc.blend_op_alpha ~= nil then
							source:write("\t\t\t\t.blend_op_alpha = RhiBlendOp_" .. desc.blend_op_alpha .. ",\n")
						end
						
						if desc.logic_op ~= nil then
							source:write("\t\t\t\t.logic_op = RhiLogicOp_" .. desc.logic_op .. ",\n")
						end

						-- TODO: Handle write mask properly
						if desc.write_mask ~= nil then
							source:write("\t\t\t\t.write_mask = RhiColorWriteEnable_" .. desc.write_mask .. ",\n")
						else
							source:write("\t\t\t\t.write_mask = RhiColorWriteEnable_all,\n")
						end
					end

					for i, rt in ipairs(pso.render_targets) do
						if rt.blend ~= nil then
							source:write("\t\t\t.blend.render_target[" .. (i - 1) .. "] = {\n")
							emit_rt_blend(rt.blend)
							source:write("\t\t\t},\n")
						else
							source:write("\t\t\t.blend.render_target[" .. (i - 1) .. "].write_mask = RhiColorWriteEnable_all,\n")
						end
					end
				end

				-- rasterizer
				if pso.fill_mode ~= nil then
					source:write("\t\t\t.rasterizer.fill_mode = RhiFillMode_" .. pso.fill_mode .. ",\n")
				end

				if pso.cull_mode ~= nil then
					source:write("\t\t\t.rasterizer.cull_mode = RhiCullMode_" .. pso.cull_mode .. ",\n")
				end

				if pso.front_winding ~= nil then
					source:write("\t\t\t.rasterizer.front_winding = RhiFrontWinding_" .. pso.front_winding .. ",\n")
				end

				if pso.depth_clip == true then
					source:write("\t\t\t.rasterizer.depth_clip_enable = true,\n")
				end

				if pso.multisample == true then
					source:write("\t\t\t.rasterizer.multisample_enable = true,\n")
				end

				if pso.anti_aliased_lines == true then
					source:write("\t\t\t.rasterizer.anti_aliased_line_enable = true,\n")
				end

				if pso.conservative_rasterization == true then
					source:write("\t\t\t.rasterizer.conservative_rasterization = true,\n")
				end

				-- depth stencil
				if pso.depth_test == true then
					source:write("\t\t\t.depth_stencil.depth_test_enable = true,\n")
				end

				if pso.depth_write == true then
					source:write("\t\t\t.depth_stencil.depth_write = true,\n")
				end

				if pso.depth_func ~= nil then
					source:write("\t\t\t.depth_stencil.depth_func = RhiComparisonFunc_" .. pso.depth_func .. ",\n")
				end

				if pso.stencil_test == true then
					source:write("\t\t\t.depth_stencil.stencil_test_enable = true,\n")
				end

				if pso.stencil_read_mask ~= nil then
					source:write("\t\t\t.depth_stencil.stencil_read_mask = " .. pso.stencil_read_mask .. ",\n")
				end

				if pso.stencil_write_mask ~= nil then
					source:write("\t\t\t.depth_stencil.stencil_write_mask = " .. pso.stencil_write_mask .. ",\n")
				end

				local function emit_depth_stencil_op_desc(desc)
					if desc.fail_op ~= nil then
						source:write("\t\t\t\t.stencil_fail_op = RhiStencilOp_" .. desc.fail_op .. ",\n")
					end

					if desc.depth_fail_op ~= nil then
						source:write("\t\t\t\t.stencil_depth_fail_op = RhiStencilOp_" .. desc.depth_fail_op .. ",\n")
					end

					if desc.pass_op ~= nil then
						source:write("\t\t\t\t.stencil_pass_op = RhiStencilOp_" .. desc.pass_op .. ",\n")
					end

					if desc.func ~= nil then
						source:write("\t\t\t\t.stencil_func = RhiComparisonFunc_" .. desc.func .. ",\n")
					end
				end

				if pso.stencil_front ~= nil then
					source:write("\t\t\t.front = {\n")
					emit_depth_stencil_op_desc(pso.stencil_front)
				end

				if pso.stencil_back ~= nil then
					source:write("\t\t\t.back = {\n")
					emit_depth_stencil_op_desc(pso.stencil_back)
				end

				--

				if pso.topology ~= nil then
					source:write("\t\t\t.primitive_topology_type = RhiPrimitiveTopologyType_" .. pso.topology .. ",\n")
				else
					source:write("\t\t\t.primitive_topology_type = RhiPrimitiveTopologyType_triangle,\n")
				end

				if pso.render_targets ~= nil then
					local rt_count = #pso.render_targets

					if rt_count == 0 then 
						emit.error("A render targets array was specified, but there are no valid render targets.")
					end

					source:write("\t\t\t.render_target_count = " .. rt_count .. ",\n")
					
					source:write("\t\t\t.rtv_formats = {\n")

					for i, rt in ipairs(pso.render_targets) do
						if rt.pf == nil then
							emit.error("A render target must specify its pixel format ('pf').")
						end

						source:write("\t\t\t\tPixelFormat_" .. rt.pf .. ",\n")
					end

					source:write("\t\t\t},\n")
				end

				if pso.dsv_format ~= nil then
					source:write("\t\t\t.dsv_format = PixelFormat_" .. pso.dsv_format .. ",\n")
				end

				if pso.multisample_count ~= nil then
					source:write("\t\t\t.multisample_count = " .. tostring(pso.multisample_count) .. ",\n")
				else
					source:write("\t\t\t.multisample_count = 1,\n")
				end

				if pso.multisample_quality ~= nil then
					source:write("\t\t\t.multisample_quality = " .. tostring(pso.multisample_quality) .. ",\n")
				end
				
				source:write("\t\t},\n")

				--

				source:write("\t},\n")
			end
		end
	end

	source:write("};\n\n")

	source:close()
end

function process_shaders(p)
	local bundles = p.bundles

	local bundles_filtered = {}

	for _, bundle_info in ipairs(bundles) do
		local bundle = bundle_info.bundle
		assert(bundle)

		if bundle.disabled ~= true then
			table.insert(bundles_filtered, bundle_info)
		end
	end

	p.bundles = bundles_filtered

	process_shaders_internal(p)
end