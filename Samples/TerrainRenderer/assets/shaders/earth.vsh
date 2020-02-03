Texture2D<float> g_heightMap;
SamplerState     g_heightMap_sampler; // By convention, texture samplers must use the '_sampler' suffix

struct GlobalConstants
{
    float4x4 g_worldViewProj;
    float4x4 g_worldView;
    float3   g_lightPosition;
    float    g_heightScale;
    float    g_gridOffset;
};

cbuffer VSConstants
{
    GlobalConstants constants;
};

// Vertex shader takes two inputs: vertex position and color.
// By convention, Diligent Engine expects vertex shader inputs to be 
// labeled 'ATTRIBn', where n is the attribute number.
struct VSInput
{
    // UV also used for coords, less memory
    float2 UV   : ATTRIB0; 
};

struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float2 UV    : TEX_COORD;
    float  NdotL : N_DOT_L;
};

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    // Pixel Pos
    float height = g_heightMap.SampleLevel(g_heightMap_sampler, VSIn.UV, 0) * constants.g_heightScale;
    PSIn.Pos = mul( float4(VSIn.UV, height, 1.0), constants.g_worldViewProj);

    // Pixel UV
    PSIn.UV = VSIn.UV;

    // Pixel N dot L
    // finite difference for the bump mapping
    float offset = constants.g_gridOffset;

    float2 L = float2(max(VSIn.UV[0] - offset, 0.0f), VSIn.UV[1]);
    float2 R = float2(min(VSIn.UV[0] + offset, 1.0f), VSIn.UV[1]);
    float2 T = float2(VSIn.UV[0], min(VSIn.UV[1] + offset, 1.0f));
    float2 B = float2(VSIn.UV[0], max(VSIn.UV[1] - offset, 0.0f));

    float heightL = g_heightMap.SampleLevel(g_heightMap_sampler, L, 0)* constants.g_heightScale;
    float heightR = g_heightMap.SampleLevel(g_heightMap_sampler, R, 0)* constants.g_heightScale;
    float heightT = g_heightMap.SampleLevel(g_heightMap_sampler, T, 0)* constants.g_heightScale;
    float heightB = g_heightMap.SampleLevel(g_heightMap_sampler, B, 0)* constants.g_heightScale;

    float4 vertPos  = float4(VSIn.UV, height, 1.0);
    float4 lightPos = float4(constants.g_lightPosition, 1.0);
    float4 lightDirection = normalize(vertPos - lightPos);

    float3 normal = normalize(float3(2*(heightR - heightL), 2*(heightB - heightT), -4));

    PSIn.NdotL = saturate(dot(normal.xyz, lightDirection.xyz));

    // lamp effect
    if (PSIn.NdotL < sqrt(3) / 2)
    {
        PSIn.NdotL = 0.0;
    }
}
