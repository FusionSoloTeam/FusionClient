#version 450

layout(set = 0, binding = 0) uniform sampler2D g_Previous;
layout(set = 1, binding = 0) uniform sampler2D g_Current;
layout(set = 2, binding = 0) uniform sampler2D g_Motion;
layout(push_constant) uniform SParams
{
	float m_Phase;
} g_Params;
layout(location = 0) noperspective in vec2 v_Uv;
layout(location = 0) out vec4 FragColor;

vec4 MotionAt(vec2 Uv)
{
	return (textureLod(g_Motion, Uv, 0.0) * 255.0 - 128.0) * 0.5;
}

bool InFrame(vec2 Uv)
{
	return all(greaterThanEqual(Uv, vec2(0.0))) && all(lessThanEqual(Uv, vec2(1.0)));
}

void main()
{
	float Phase = g_Params.m_Phase;
	if(Phase >= 1.0)
	{
		// no next frame to blend toward, so the last present is the real frame
		FragColor = texelFetch(g_Current, ivec2(gl_FragCoord.xy), 0);
		return;
	}

	vec2 Texel = 1.0 / vec2(textureSize(g_Current, 0));
	vec4 Motion = MotionAt(v_Uv);
	vec2 PrevUv = v_Uv - Phase * Motion.xy * Texel;
	vec2 CurrUv = v_Uv - (1.0 - Phase) * Motion.zw * Texel;
	vec4 Prev = textureLod(g_Previous, PrevUv, 0.0);
	vec4 Curr = textureLod(g_Current, CurrUv, 0.0);
	if(!InFrame(PrevUv) || !InFrame(CurrUv))
	{
		FragColor = InFrame(CurrUv) ? Curr : Prev;
		return;
	}

	float Diff = max(max(abs(Prev.r - Curr.r), abs(Prev.g - Curr.g)), abs(Prev.b - Curr.b));
	if(Diff < 0.08)
	{
		FragColor = mix(Prev, Curr, Phase);
		return;
	}

	// occlusions and cuts: pick the better-supported source, not both silhouettes
	float PrevError = length(Motion.xy + MotionAt(PrevUv + Motion.xy * Texel).zw);
	float CurrError = length(Motion.zw + MotionAt(CurrUv + Motion.zw * Texel).xy);
	if(abs(PrevError - CurrError) < 1.0)
		FragColor = Phase < 0.5 ? Prev : Curr;
	else
		FragColor = PrevError < CurrError ? Prev : Curr;
}
