(cbuffer pass 1
	(positions     (StructuredBuffer float3))
	(uvs           (StructuredBuffer float2))
	(lm_uvs        (StructuredBuffer float2))
	(normals       (StructuredBuffer float3))
	(sun_shadowmap (Texture2D        float ))
	(shadowmap_dim float2))

(cbuffer draw 0
	(albedo       (Texture2D float3))
	(albedo_dim   float2)
	(lightmap     (Texture2D float3))
	(lightmap_dim float2)
	(normal       float3))

