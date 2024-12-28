struct VertexShaderOutput
{
    float4 clipSpacePosition : SV_POSITION;
    float3 viewSpacePosition : POSITION0;
    float3 objectSpacePosition : POSITION1;
    float3 worldSpacePosition : POSITION2;
    float3 viewSpaceNormal : NORMAL;
    float3 viewSpaceTangent : TANGENT;
    float3 viewSpaceBitangent : BITANGENT;
    float2 texCoord : TEXCOORD;
};

struct PointLight
{
    float3 position;
    float3 lightColor;
    float lightIntensity;
};

/// <summary>
/// Constants that can change every frame.
/// </summary>
cbuffer PerFrameConstants : register(b0)
{
    float4x4 projectionMatrix;
    float shadowBias;
}

/// <summary>
/// Constants that can change per Mesh/Drawcall.
/// </summary>
cbuffer PerMeshConstants : register(b1)
{
    float4x4 modelViewMatrix;
    float4x4 modelMatrix;
}

/// <summary>
/// Constants that are really constant for the entire scene.
/// </summary>
cbuffer Material : register(b2)
{
    float4 ambientColor;
    float4 diffuseColor;
    float4 specularColorAndExponent;
}

cbuffer LightBuffer : register(b3)
{
    PointLight light[8];
    uint numLights;
}

Texture2D<float4> g_textureAmbient : register(t0);
Texture2D<float4> g_textureDiffuse : register(t1);
Texture2D<float4> g_textureSpecular : register(t2);
Texture2D<float4> g_textureEmissive : register(t3);
Texture2D<float4> g_textureNormal : register(t4);

SamplerState g_sampler : register(s0);

RaytracingAccelerationStructure TLAS : register(t5, space0); // Acceleration structure

struct MyCustomAttrIntersectionAttributes
{
    float4 baseColor;
    float3 normal;
};

VertexShaderOutput VS_main(float3 position : POSITION, float3 normal : NORMAL, float3 tangent : TANGENT, float2 texCoord : TEXCOORD)
{
    VertexShaderOutput output;
    float4 p4 = mul(modelViewMatrix, float4(position, 1.0f));
    output.objectSpacePosition = position;
    output.worldSpacePosition = mul(modelMatrix, float4(position, 1.0f));
    output.viewSpacePosition = p4.xyz;
    output.viewSpaceNormal = normal;
    output.clipSpacePosition = mul(projectionMatrix, p4);
    output.texCoord = texCoord;
    output.viewSpaceTangent = mul((float3x3) modelViewMatrix, tangent);
    output.viewSpaceBitangent = cross(output.viewSpaceNormal, output.viewSpaceTangent);
    return output;
}

// Simple hash function to generate pseudo-random values
float Rand(float2 uv)
{
    // Simple hash function using bitwise operations and trigonometry
    uv = frac(uv * float2(12.9898, 78.233)); // Scale input for variety
    float v = dot(uv, float2(127.1, 311.7)); // Dot product with arbitrary constants
    return frac(sin(v) * 43758.5453); // Apply sine function and scale
}

float4 PS_main(VertexShaderOutput input)
    : SV_TARGET
{
    float3 accumulatedLightContribution = float3(0.0, 0.0, 0.0); // Initialize output color
    // Number of rays to sample for soft shadows
    uint numShadowRays = 1; // More rays = softer shadow
    float shadowFactor = 0.0f; // Initialize shadow factor
    bool anyHit = false;
    
    for (uint i = 0; i < numLights; i++)
    {
        float3 currentLightContribution = float3(0.0, 0.0, 0.0); // Initialize light contribution
        PointLight l = light[i];
        float3 testPos = l.position;
        float3 lightDir = normalize(testPos - input.worldSpacePosition.xyz);
        float distance = length(testPos - input.worldSpacePosition.xyz);
        float attenuation = 1.0 / distance;
        float nDotL = max(0.0f, dot(normalize(input.viewSpaceNormal), lightDir));
        float4 diffuse = g_textureDiffuse.Sample(g_sampler, input.texCoord) * diffuseColor * nDotL;

        // Apply attenuation
        diffuse *= attenuation * l.lightIntensity;
        
        // apply light color
        diffuse *= float4(l.lightColor.xyz, 1.0f);

        // Soft Shadow Calculation: Trace multiple rays
        float bias = 0.0001f;
        float raysHit = 0;

        // Trace multiple rays from the point towards the light source
        for (uint j = 0; j < numShadowRays; j++)
        {
            // Generate a random offset using pixel coordinates (or other unique inputs)
            float2 uv = input.texCoord.xy + float2(j, i) * 0.1f; // Add unique offset based on ray index
            float randomX = Rand(uv); // Random value for X
            float randomY = Rand(uv + float2(1.0, 0.0)); // Random value for Y (offset by 1 for variation)
            float3 randomOffset = float3(randomX * 2.0f - 1.0f, randomY * 2.0f - 1.0f, 0.0f); // Random jitter in X and Y

            // Ray from the surface point to the light
            RayDesc ray;
            ray.Origin = input.worldSpacePosition.xyz + shadowBias * normalize(input.viewSpaceNormal.xyz); // Offset to avoid self-intersection
            ray.Direction = normalize(lightDir); // Add jitter to the light direction
            ray.TMin = bias;
            ray.TMax = distance;

            RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES > q;
            q.TraceRayInline(TLAS, 0, 0xFF, ray);

            // Check if the ray hits something (shadow hit)
            if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
            {
                raysHit += 1; // Count how many rays hit an object
                anyHit = true;
            }
        }
        
        // Calculate the shadow factor (percentage of rays that hit)
        shadowFactor = raysHit / float(numShadowRays);

        // Dim the light based on the shadow factor
        diffuse.xyz *= (1.0f - shadowFactor); // The more rays that hit, the darker the shadow

        // Accumulate light contribution
        currentLightContribution += diffuse.xyz;
        accumulatedLightContribution += currentLightContribution;
    }

    if (anyHit)
        return float4(1.0f, 0.0f, 0.0f, 1.0f);
    else
        return float4(0.0f, 0.0f, 1.0f, 1.0f);
    
        
        
        
        //return float4(accumulatedLightContribution.xyz, 1.0f);
}