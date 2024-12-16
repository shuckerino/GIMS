static const float3 vertices[] = { { 0.0f, 0.25f, 0.0f }, { 0.25f, -0.25f, 0.0f }, { -0.25f, -0.25f, 0.0f } };

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
};

RaytracingAccelerationStructure Scene : register(t0, space0); // Acceleration structure

VertexShaderOutput VS_main(uint i : SV_VertexID)
{
    VertexShaderOutput output;
    output.position = float4(vertices[i], 1.0f);
    return output;
}

float4 PS_main(VertexShaderOutput input) : SV_TARGET
{
    float3 lightPosition = float3(0, 0, -2);
    float3 lightColor = float3(1, 1, 1);
    
    // Compute light direction and distance
    float3 lightDir = normalize(lightPosition - input.position.xyz);
    float lightDistance = length(lightPosition - input.position.xyz);
    
    RayDesc ray;
    ray.Origin = input.position.xyz;
    ray.Direction = lightDir;
    ray.TMin = 0.001;
    ray.TMax = lightDistance;
    
    RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES > q;
    q.TraceRayInline(Scene, 0, 0xFF, ray);
    q.Proceed();
    
    float3 color = float3(0.1, 0.1, 0.1); // Ambient light
    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) // hit
    {
        // Add diffuse lighting
        float diffuse = max(dot(lightDir, float3(0, 0, 1)), 0.0);
        color += diffuse * lightColor;
    }
    //else // miss
    //{
    //    return float4(0, 0, 1, 1.0f);
    //}
    
    return float4(color, 1.0);
 
    //return float4(0.0f, 0.8f, 0.0f, 1.0f);
    
}


//[shader("raygeneration")]
//void MyRaygenShader()
//{
//    float2 lerpValues = (float2) DispatchRaysIndex() / (float2) DispatchRaysDimensions();

//    // Orthographic projection since we're raytracing in screen space.
//    float3 rayDir = float3(0, 0, 1);
//    float3 origin = float3(
//        lerp(g_rayGenCB.viewport.left, g_rayGenCB.viewport.right, lerpValues.x),
//        lerp(g_rayGenCB.viewport.top, g_rayGenCB.viewport.bottom, lerpValues.y),
//        0.0f);

//    if (IsInsideViewport(origin.xy, g_rayGenCB.stencil))
//    {
//        // Trace the ray.
//        // Set the ray's extents.
//        RayDesc ray;
//        ray.Origin = origin;
//        ray.Direction = rayDir;
//        // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
//        // TMin should be kept small to prevent missing geometry at close contact areas.
//        ray.TMin = 0.001;
//        ray.TMax = 10000.0;
//        RayPayload payload = { float4(0, 0, 0, 0) };
//        TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

//        // Write the raytraced color to the output texture.
//        RenderTarget[DispatchRaysIndex().xy] = payload.color;
//    }
//    else
//    {
//        // Render interpolated DispatchRaysIndex outside the stencil window
//        RenderTarget[DispatchRaysIndex().xy] = float4(lerpValues, 0, 1);
//    }
//}

//[shader("closesthit")]
//void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
//{
//    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
//    payload.color = float4(barycentrics, 1);
    
//    //payload.color = float4(1, 0, 0, 1); // Red
    
//}

//[shader("miss")]
//void MyMissShader(inout RayPayload payload)
//{
//    payload.color = float4(0, 0, 0, 1);
//    //payload.color = float4(1, 1, 0, 1); // Yellow
//}

