vec3 uvDepthToWorld(vec2 uv, float depth, mat4 invViewProj)
{
	float z = depth * 2.0f - 1.0f;
	vec4 ndc = vec4(uv * 2.0f - 1.0f, z, 1.0f);

	vec4 worldPos = invViewProj * ndc;
	worldPos.xyz /= worldPos.w;
	return worldPos.xyz;
}