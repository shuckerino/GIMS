struct VertexShaderOutput
{
    float4 clipSpacePosition : SV_POSITION;
    float3 viewSpacePosition : POSITION;
    float3 viewSpaceNormal : NORMAL;
    float2 texCoord : TEXCOORD;
};

/// <summary>
/// Constants that can change every frame.
/// </summary>
cbuffer PerFrameConstants : register(b0)
{
    float4x4 projectionMatrix;
}

/// <summary>
/// Constants that can change per Mesh/Drawcall.
/// </summary>
cbuffer PerMeshConstants : register(b1)
{
    float4x4 modelViewMatrix;
}

VertexShaderOutput VS_main(float3 position : POSITION, float3 normal : NORMAL, float2 texCoord : TEXCOORD)
{
    VertexShaderOutput output;

    float4 p4 = mul(modelViewMatrix, float4(position, 1.0f));
    output.viewSpacePosition = p4.xyz;
    output.viewSpaceNormal = mul(modelViewMatrix, float4(normal, 0.0f)).xyz;
    output.clipSpacePosition = mul(projectionMatrix, p4);
    output.texCoord = texCoord;

    // Skip all transformations and pass the input position directly to clip space.
    //output.clipSpacePosition = mul(modelViewMatrix, float4(position.x, position.y, 0.5f, 1.0f));
    //output.viewSpacePosition = position;              // Also pass the raw position as view-space (for debugging consistency).
    //output.viewSpaceNormal = normal;                  // Pass the normal as-is.
    //output.texCoord = texCoord;                       // Pass the texture coordinates.

    return output;
}

float4 PS_main(VertexShaderOutput input)
    : SV_TARGET
{
  //return float4(input.viewSpaceNormal.x, input.texCoord.y, 0.0f, 1.0f);
  return float4(0.2f, 0.8f, 0.8f, 1.0f);

}