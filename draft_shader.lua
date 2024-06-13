shader = {
	name = "brush",

	pass_parameters = {
		positions     = StructuredBuffer(float3),
		uvs           = StructuredBuffer(float2),
		lm_uvs        = StructuredBuffer(float2),
		normals       = StructuredBuffer(float3),
		sun_shadowmap = Texture2D(float),
		shadowmap_dim = float2,
	},

	draw_parameters = {
		albedo       = Texture2D(float3),
		albedo_dim   = float2,
		lightmap     = Texture2D(float3),
		lightmap_dim = float2,
	},

	pipeline = {
		rasterizer = {
			cull_mode     = "back",
			front_winding = "ccw",
		},

		depth_stencil = {
			depth_test  = true,
			depth_write = true,
			depth_func  = "greater",
		},

		topology = "triangle",
	},

	source = [[

	struct VS_OUT
	{
		float4 position           : SV_Position;
		float4 shadowmap_position : SHADOWMAP_POSITION;
		float2 uv                 : TEXCOORD;
		float2 lightmap_uv        : LIGHTMAP_TEXCOORD;
		float3 normal             : NORMAL;
	};

	VS_OUT MainVS(uint vertex_index : SV_VertexID)
	{
		float3 position    = pass.positions   .Get().Load(vertex_index);
		float2 uv          = pass.uvs         .Get().Load(vertex_index);
		float2 lightmap_uv = pass.lightmap_uvs.Get().Load(vertex_index);
		float3 normal      = pass.normals     .Get().Load(vertex_index);

		VS_OUT OUT;
		OUT.position           = mul(view.world_to_clip, float4(position, 1));
		OUT.shadowmap_position = mul(view.sun_matrix,    float4(position, 1));
		OUT.uv                 = uv;
		OUT.lightmap_uv        = lightmap_uv;
		OUT.normal             = normal;
		return OUT;
	}

	float4 MainPS(VS_OUT IN) : SV_Target
	{
		Texture2D albedo = draw.albedo.Get();

		float2 albedo_dim;
		albedo.GetDimensions(albedo_dim.x, albedo_dim.y);

		float3 color = albedo.Sample(df::s_aniso_wrap, FatPixel(albedo_dim, IN.uv)).rgb;

		Texture2D lightmap = draw.lightmap.Get();

		float2 lightmap_dim;
		lightmap.GetDimensions(lightmap_dim.x, lightmap_dim.y);

		float3 light = lightmap.Sample(df::s_aniso_wrap, FatPixel(lightmap_dim, IN.lightmap_uv)).rgb;

		const float3 shadow_test_pos   = IN.shadowmap_position.xyz / IN.shadowmap_position.w;
		const float2 shadow_test_uv    = 0.5 + float2(0.5, -0.5)*shadow_test_pos.xy;
		const float  shadow_test_depth = shadow_test_pos.z;

		float shadow_factor = 1.0;

		if (shadow_test_depth >= 0.0 && shadow_test_depth <= 1.0)
		{
			const float bias = max(0.025*(1.0 - max(0, dot(IN.normal, view.sun_direction))), 0.010);
			const float biased_depth = shadow_test_depth + bias;

			float2 shadowmap_dim;
			pass.sun_shadowmap.Get().GetDimensions(shadowmap_dim.x, shadowmap_dim.y);

			shadow_factor = SampleShadowPCF3x3(pass.sun_shadowmap.Get(), shadowmap_dim, shadow_test_uv, biased_depth);
		}

		light += shadow_factor*(view.sun_color*saturate(dot(IN.normal, view.sun_direction)));

		return float4(color*light, 1);
	}

	]]
}

return shader