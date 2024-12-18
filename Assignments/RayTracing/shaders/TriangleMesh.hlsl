struct VertexShaderOutput
{
    float4 clipSpacePosition : SV_POSITION;
    float3 viewSpacePosition : POSITION;
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
    float3 lightDirection;
    uint1 flags;
}

/// <summary>
/// Constants that can change per Mesh/Drawcall.
/// </summary>
cbuffer PerMeshConstants : register(b1)
{
    float4x4 modelViewMatrix;
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

//Texture2D<float4> g_textureAmbient : register(t0);
//Texture2D<float4> g_textureDiffuse : register(t1);
//Texture2D<float4> g_textureSpecular : register(t2);
//Texture2D<float4> g_textureEmissive : register(t3);
//Texture2D<float4> g_textureNormal : register(t4);

//SamplerState g_sampler : register(s0);

RaytracingAccelerationStructure TLAS : register(t0, space0); // Acceleration structure


VertexShaderOutput VS_main(float3 position : POSITION, float3 normal : NORMAL, float3 tangent : TANGENT, float2 texCoord : TEXCOORD)
{
    VertexShaderOutput output;

    float4 p4 = mul(modelViewMatrix, float4(position, 1.0f));
    output.viewSpacePosition = p4.xyz;
    output.viewSpaceNormal = mul(modelViewMatrix, float4(normal, 0.0f)).xyz;
    output.clipSpacePosition = mul(projectionMatrix, p4);
    output.texCoord = texCoord;

    output.viewSpaceTangent = mul((float3x3) modelViewMatrix, tangent);
    output.viewSpaceBitangent = cross(output.viewSpaceNormal, output.viewSpaceTangent);
    return output;
}

float4 PS_main(VertexShaderOutput input)
    : SV_TARGET
{
    float3 lightPosition = float3(0, 0, -2);
    float3 lightColor = float3(1, 1, 1);
    
    // Compute light direction and distance
    float3 lightDir = normalize(lightPosition - input.clipSpacePosition.xyz);
    float lightDistance = length(lightPosition - input.clipSpacePosition.xyz);
    
    RayDesc ray;
    ray.Origin = input.clipSpacePosition.xyz;
    ray.Direction = lightDir;
    ray.TMin = 0.001;
    ray.TMax = lightDistance;
    
    RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES > q;
    q.TraceRayInline(TLAS, 0, 0xFF, ray);
    float3 color = float3(0.1, 0.1, 0.1); // Ambient light
    
    while (q.Proceed())
    {
        if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
                    // Get the hit attributes
            //float3 hitPosition = q.CommittedPosition();
            //float3 hitNormal = normalize(q.CommittedNormal());
    
        // Calculate diffuse lighting
            float diffuse = max(dot(input.viewSpaceNormal, lightDir), 0.0);
            color += diffuse * lightColor;
        }
        else // miss
        {
            color = float3(0.1, 0.1, 0.1); // Ambient light
        }
    }
    
    return float4(color, 1.0);
    //return float4(1.0f, 0.0f, 0.0f, 1.0f);
 
}