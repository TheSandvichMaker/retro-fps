@shader triangle
{
	@parameters(slot=0) draw
	{
		offset : float4
		color  : float4
	}

	@parameters(slot=1) pass
	{
		positions : StructuredBuffer<float3>
		colors    : StructuredBuffer<float4>
	}

	source(entry_vs=MainVS, entry_ps=MainPS): {
		struct VS_OUT
		{
			float4 position : SV_Position;
			float4 color    : COLOR;
		};

		VS_OUT MainVS(uint vertex_index : SV_VertexID)
		{
			float3 position = pass.positions.Get().Load(vertex_index);
			float4 color    = pass.colors   .Get().Load(vertex_index);

			VS_OUT OUT;
			OUT.position = float4(position, 1.0) + draw.offset;
			OUT.color    = draw.color;
			return OUT;
		}

		float4 MainPS(VS_OUT IN) : SV_Target
		{
			return IN.color;
		}
	}
}

