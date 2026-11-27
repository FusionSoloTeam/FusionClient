#version 450

layout(set = 0, binding = 0) uniform sampler2D g_Previous;
layout(set = 1, binding = 0) uniform sampler2D g_Current;
layout(location = 0) noperspective in vec2 v_Uv;
layout(location = 0) out vec4 FragColor;

const vec2 aDirections[8] = vec2[8](vec2(-1, 0), vec2(1, 0), vec2(0, -1), vec2(0, 1),
	vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, 1));
const vec2 aTaps[5] = vec2[5](vec2(0, 0), vec2(-4, -4), vec2(4, -4), vec2(-4, 4), vec2(4, 4));
const float aSteps[5] = float[5](24.0, 12.0, 6.0, 3.0, 1.0);

float Match(sampler2D Target, vec2 Uv, vec2 Texel, vec3 aReference[5], vec2 Offset)
{
	vec2 Center = Uv + Offset * Texel;
	if(any(lessThan(Center, vec2(0.0))) || any(greaterThan(Center, vec2(1.0))))
		return 2.0;

	// ties go to the smaller motion, so tiled maps do not ghost
	float Error = 0.0015 * length(Offset);
	for(int Tap = 0; Tap < 5; ++Tap)
	{
		vec3 Delta = abs(aReference[Tap] - textureLod(Target, Center + aTaps[Tap] * Texel, 0.0).rgb);
		Error += dot(Delta, vec3(0.066667));
	}
	return Error;
}

vec2 Estimate(sampler2D Source, sampler2D Target, vec2 Texel)
{
	vec3 aReference[5];
	for(int Tap = 0; Tap < 5; ++Tap)
		aReference[Tap] = textureLod(Source, v_Uv + aTaps[Tap] * Texel, 0.0).rgb;

	vec2 Best = vec2(0.0);
	float BestError = Match(Target, v_Uv, Texel, aReference, Best);
	if(BestError < 0.006)
		return Best;

	for(int Level = 0; Level < 5; ++Level)
	{
		vec2 Start = Best;
		for(int Direction = 0; Direction < 8; ++Direction)
		{
			vec2 Candidate = Start + aDirections[Direction] * aSteps[Level];
			float Error = Match(Target, v_Uv, Texel, aReference, Candidate);
			if(Error < BestError)
			{
				Best = Candidate;
				BestError = Error;
			}
		}
	}
	return Best;
}

void main()
{
	vec2 Texel = 1.0 / vec2(textureSize(g_Current, 0));
	vec2 Forward = Estimate(g_Previous, g_Current, Texel);
	vec2 Backward = Estimate(g_Current, g_Previous, Texel);
	// 128/255 must round-trip to zero, or static regions drift
	FragColor = (vec4(Forward, Backward) * 2.0 + 128.0) / 255.0;
}
