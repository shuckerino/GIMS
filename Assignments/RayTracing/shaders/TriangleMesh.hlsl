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

/// <summary>
/// Constants that can change every frame.
/// </summary>
cbuffer PerFrameConstants : register(b0)
{
    float4x4 projectionMatrix;
    float3 lightPosition;
    float lightIntensity;
    float shadowBias;
    uint1 flags;
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

float4 PS_main(VertexShaderOutput input)
    : SV_TARGET
{
    
    float3 lightDir = normalize(lightPosition - input.worldSpacePosition.xyz);
    float distance = length(lightPosition - input.worldSpacePosition.xyz);
    float lightIntensity = 50.0f;
    float attenuation = 1.0 / distance;
    bool useRayTracing = (flags & 0x1);
    float3 finalColor = float3(0.0, 0.0, 1.0); // Initialize output color
    bool hit = false;
    
    if (useRayTracing)
    {
        finalColor = float3(0.0, 0.0, 1.0); // Miss color for ray tracing is blue
        RayDesc ray;
        ray.Origin = input.worldSpacePosition.xyz + shadowBias * normalize(input.viewSpaceNormal.xyz); // add shadow bias to avoid artifacts
        ray.Direction = lightDir;
        ray.TMin = 0.0001;
        ray.TMax = 1e6;
    
        RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES > q;
        q.TraceRayInline(TLAS, 0, 0xFF, ray);
        float3 hitPosition = float3(0.0f, 0.0f, 0.0f);
    
        // traverse TLAS
        q.Proceed();
    
        if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            hit = true;
        }
    }

    //float n = normalize(input.viewSpaceNormal);
    //float3 v = normalize(-input.viewSpacePosition);
    //float3 h = normalize(lightDir + v);

    //float f_diffuse = max(0.0f, dot(n, lightDir));

    //float4 diffuse = g_textureDiffuse.Sample(g_sampler, input.texCoord) * diffuseColor;
    //finalColor = diffuse * attenuation;
    
    // Calculate diffuse lighting
    float nDotL = max(0.0f, dot(normalize(input.viewSpaceNormal), lightDir));
    float4 diffuse = g_textureDiffuse.Sample(g_sampler, input.texCoord) * diffuseColor * nDotL;

    // Apply attenuation
    diffuse *= attenuation * lightIntensity;

    finalColor = diffuse.xyz;
    
    if (hit) // is in shadow
    {
        finalColor *= 0.8f;
        //finalColor = float3(1.0, 0.0, 0.0); // Initialize output color
        
    }

    return float4(finalColor.xyz, 1.0f);
}