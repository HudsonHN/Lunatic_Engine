#define NUM_CASCADES 4
#define MAX_BONE_INFLUENCES 4

struct MaterialConstants 
{
	vec4 colorFactors;
	vec4 metalRoughFactors;
};

struct ImageMetaInfo 
{
	MaterialConstants constants;
};

struct DeferredLightingConstants
{
	int bDrawShadowMaps;
	int bDrawReflectiveShadowMaps;
	int numReflectShadowSamples;
	int bDrawSsao;

	float sampleRadius;
	float reflectiveIntensity;
	float ambientScale;
	float minBiasFactor;
	float maxBiasFactor;
};

struct SceneData{   
	mat4 view;
	mat4 proj;
	mat4 viewProj;
	vec4 ambientColor;
	vec4 sunlightDirection;
	vec4 sunlightColor;
	vec3 cameraPos;
	uint numLights;
	vec2 jitterOffset;
	uint bApplyTAA;
	float renderScale;
	float deltaTime;
	uint frameIndex;
};

struct Light
{
	vec3 position;
	vec3 color;
	float intensity;
	vec3 direction;
	float radius;
	uint type;
};

struct Bounds 
{
	vec3 origin;
	vec3 extents;
	//vec3 maxPos;
	float sphereRadius;
};

struct SurfaceInfo 
{
	mat4 worldTransform;
	Bounds bounds;
	uint surfaceIndex;
	uint materialIndex;
	uint drawIndex;
	uint bIsTransparent;
	uint bIsSkinned;
	uint boneOffset;
	uint boneCount;
};

struct Vertex
{
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
	vec4 tangent;

	int boneIndices[MAX_BONE_INFLUENCES];
	float boneWeights[MAX_BONE_INFLUENCES];
};

struct VkDrawIndexedIndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

struct Plane {
	vec3 normal;
	float distance;
};