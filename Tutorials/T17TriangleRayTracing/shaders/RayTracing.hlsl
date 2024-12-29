static const float3 colors[] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float4 worldPosition : POSITION0;
    float4 color : COLOR;
};

struct VertexShaderInput
{
    float3 position : POSITION;
    uint i : SV_VertexID;
    uint instanceID : SV_InstanceID;
};

struct PerInstanceData
{
    float4x4 worldTrafo : INSTANCE_DATA;
};

cbuffer PerFrameConstants : register(b0)
{
    float4x4 mvp;
    float3 lightDirection;
    uint flags;
};

RaytracingAccelerationStructure TLAS : register(t0, space0); // Acceleration structure

VertexShaderOutput VS_main(VertexShaderInput input, PerInstanceData instanceData)
{
    VertexShaderOutput output;
    output.worldPosition = mul(instanceData.worldTrafo, float4(input.position, 1.0f));
    output.position = mul(mvp, output.worldPosition);
    bool drawPlane = flags & 0x1;
    if (drawPlane)
    {
        output.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    else
    {
        output.color = float4(colors[input.instanceID], 1.0f);
    }

    return output;
}

float4 PS_main(VertexShaderOutput input) : SV_TARGET
{
    float3 lightDir = normalize(lightDirection);
    float3 outputColor = input.color.rgb;
    
    RayDesc ray;
    ray.Origin = input.worldPosition;
    ray.Direction = lightDir;
    ray.TMin = 0.001;
    ray.TMax = 1e6;
    
    RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES > q;
    q.TraceRayInline(TLAS, 0, 0xFF, ray);
    q.Proceed();
    
    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) // hit
    {
        //outputColor *= 0.6f;
        outputColor = float3(0.5f, 0.5f, 0.5f);
    }

    return float4(outputColor.rgb, 1.0f);
    
}