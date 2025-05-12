struct PS_INPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 PSMain(PS_INPUT input) : SV_TARGET {
	return float4(input.tex, 0.0f, 1.0f); // Just visualize UVs
}
