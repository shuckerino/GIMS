#include "RayTracingRenderer.hpp"

using namespace gims;

// constant shader names
const wchar_t* RayTracingRenderer::c_hitGroupName         = L"MyHitGroup";
const wchar_t* RayTracingRenderer::c_raygenShaderName     = L"MyRaygenShader";
const wchar_t* RayTracingRenderer::c_closestHitShaderName = L"MyClosestHitShader";
const wchar_t* RayTracingRenderer::c_missShaderName       = L"MyMissShader";

#pragma region Helper functions

void RayTracingRenderer::AllocateUAVBuffer(ui64 bufferSize, ID3D12Resource** ppResource,
                                           D3D12_RESOURCE_STATES initialResourceState, const wchar_t* resourceName)
{
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Alignment           = 0;
  desc.Width               = bufferSize;
  desc.Height              = 1;
  desc.DepthOrArraySize    = 1;
  desc.MipLevels           = 1;
  desc.Format              = DXGI_FORMAT_UNKNOWN;
  desc.SampleDesc.Count    = 1;
  desc.SampleDesc.Quality  = 0;
  desc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

  throwIfFailed(getDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &desc,
                                                     initialResourceState, nullptr, IID_PPV_ARGS(ppResource)));
  (*ppResource)->SetName(resourceName);
}

void RayTracingRenderer::AllocateUploadBuffer(void* initData, ui64 bufferSize, ID3D12Resource** ppResource,
                                              const wchar_t* resourceName)
{
  auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto bufferDesc           = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
  throwIfFailed(getDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                     IID_PPV_ARGS(ppResource)));
  if (resourceName)
  {
    (*ppResource)->SetName(resourceName);
  }
  void* pMappedData;
  (*ppResource)->Map(0, nullptr, &pMappedData);
  memcpy(pMappedData, initData, bufferSize);
  (*ppResource)->Unmap(0, nullptr);
}

#pragma endregion

#pragma region Init

RayTracingRenderer::RayTracingRenderer(const DX12AppConfig createInfo)
    : DX12App(createInfo)
{
  throwIfFailed(getDevice()->QueryInterface(IID_PPV_ARGS(&m_dxrDevice)));

  if (isRayTracingSupported() == false)
  {
    throw std::runtime_error("Ray tracing not supported on this device");
  }

  createRayTracingResources();

  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
  {
    debugController->EnableDebugLayer();
  }
}

bool RayTracingRenderer::isRayTracingSupported()
{
  D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
  throwIfFailed(getRTDevice()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
  if (options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
  {
    std::cout << "Ray tracing not supported on this device" << std::endl;
    return false;
  }
  else
  {
    std::cout << "Ray tracing supported on your device" << std::endl;
    return true;
  }
}

/// <summary>
/// Initializing all resources needed for ray tracing
/// </summary>
void RayTracingRenderer::createRayTracingResources()
{
  m_rayGenCB.viewport = {-1.0f, -1.0f, 1.0f, 1.0f};
  float border        = 0.1f;
  float aspectRatio  = static_cast<float>(getHeight()) / static_cast<float>(getWidth());
  if (getWidth() <= getHeight())
  {
    m_rayGenCB.stencil = {-1 + border, -1 + border * aspectRatio, 1.0f - border, 1 - border * aspectRatio};
  }
  else
  {
    m_rayGenCB.stencil = {-1 + border / aspectRatio, -1 + border, 1 - border / aspectRatio, 1.0f - border};
  }

  createRayTracingInterfaces();
  createRootSignatures();
  createRayTracingPipeline(); // not working yet

  createGeometry();
  createDescriptorHeap();
  createShaderTables();
  createAccelerationStructures();
  createOutputResource();
}

/// <summary>
/// Method for creating ID3D12Device5 and ID3D12GraphicsCommandList4 interfaces
/// </summary>
void RayTracingRenderer::createRayTracingInterfaces()
{
  throwIfFailed(
      m_dxrDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_dxrCommandAllocator)));
  m_dxrCommandLists.resize(3);
  for (ui8 i = 0; i < 3; i++)
  {
    throwIfFailed(getCommandListAtIndex(i).As(&m_dxrCommandLists[i]));
  }

  std::cout << "Created ID3D12Device5 and ID3D12GraphicsCommandList4 interfaces" << std::endl;
}

/// <summary>
/// Method for creating global and local root signatures
/// </summary>
void RayTracingRenderer::createRootSignatures()
{
  // First we create global root signature (shared across all shaders)
  {
    CD3DX12_DESCRIPTOR_RANGE UAVDescriptor;
    UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_ROOT_PARAMETER rootParameters[2];
    rootParameters[0].InitAsDescriptorTable(1, &UAVDescriptor);
    rootParameters[1].InitAsShaderResourceView(0);
    CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
    // SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &m_globalRootSignature);
    ComPtr<ID3DBlob> rootBlob, errorBlob;
    D3D12SerializeRootSignature(&globalRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob);

    getDevice()->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(),
                                     IID_PPV_ARGS(&m_globalRootSignature));

    std::cout << "Created global root signature" << std::endl;
  }

  // Then we create local root signatures for each shader
  {
    CD3DX12_ROOT_PARAMETER rootParameters[1];
    rootParameters[0].InitAsConstants(SizeOfInUint32(m_rayGenCB), 0, 0);
    CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
    localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    ComPtr<ID3DBlob> rootBlob, errorBlob;
    D3D12SerializeRootSignature(&localRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob);
    getDevice()->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(),
                                     IID_PPV_ARGS(&m_LocalRootSignature));

    std::cout << "Created local root signature" << std::endl;
  }
}

// Local root signature and shader association
// This is a root signature that enables a shader to have unique arguments that come from shader tables.
void RayTracingRenderer::CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
  // Hit group and miss shaders in this sample are not using a local root signature and thus one is not associated with
  // them.

  // Local root signature to be used in a ray gen shader.
  {
    auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    localRootSignature->SetRootSignature(m_LocalRootSignature.Get());
    // Shader association
    auto rootSignatureAssociation =
        raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
    rootSignatureAssociation->AddExport(L"MyRaygenShader");
  }
}

void RayTracingRenderer::createRayTracingPipeline()
{
  ComPtr<IDxcBlob> rayGenBlob, missBlob, hitBlob;

  rayGenBlob =
      compileShader(L"../../../Tutorials/T17TriangleRayTracing/Shaders/Triangle.hlsl", L"MyRaygenShader", L"lib_6_3");
  auto rayGenShaderByteCode = HLSLCompiler::convert(rayGenBlob);

  missBlob =
      compileShader(L"../../../Tutorials/T17TriangleRayTracing/Shaders/Triangle.hlsl", L"MyMissShader", L"lib_6_3");
  auto missShaderByteCode = HLSLCompiler::convert(missBlob);

  hitBlob = compileShader(L"../../../Tutorials/T17TriangleRayTracing/Shaders/Triangle.hlsl", L"MyClosestHitShader",
                          L"lib_6_3");

  auto hitShaderByteCode = HLSLCompiler::convert(hitBlob);

  CD3DX12_STATE_OBJECT_DESC raytracingPipeline {D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE};

  //auto                  lib     = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
  //D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)Triangle_cso, ARRAYSIZE(Triangle_cso));
  //lib->SetDXILLibrary(&libdxil);
  // Define which shader exports to surface from the library.
  // If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
  // In this sample, this could be omitted for convenience since the sample uses all shaders in the library.
  //{
  //  lib->DefineExport(c_raygenShaderName);
  //  lib->DefineExport(c_closestHitShaderName);
  //  lib->DefineExport(c_missShaderName);
  //}

  // DXIL (intermediate language) Library Subobject for RayGen
  auto rayGenLibrary = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
  rayGenLibrary->SetDXILLibrary(&rayGenShaderByteCode);
  rayGenLibrary->DefineExport(L"MyRaygenShader");

  // DXIL Library Subobject for Miss
  auto missLibrary = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
  missLibrary->SetDXILLibrary(&missShaderByteCode);
  missLibrary->DefineExport(L"MyMissShader");

  // DXIL Library Subobject for Hit
  auto hitLibrary = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
  hitLibrary->SetDXILLibrary(&hitShaderByteCode);
  hitLibrary->DefineExport(L"MyClosestHitShader");

  // Triangle hit group
  // A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the
  // geometry's triangle/AABB. In this sample, we only use triangle geometry with a closest hit shader, so others are
  // not set.
  auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
  hitGroup->SetClosestHitShaderImport(L"MyClosestHitShader");
  hitGroup->SetHitGroupExport(L"MyHitGroup");
  hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

  // Define Shader Config
  auto shaderConfig  = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
  UINT payloadSize   = 4 * sizeof(float); // float4 for color
  UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
  shaderConfig->Config(payloadSize, attributeSize);

  // Local root signature and shader association
  CreateLocalRootSignatureSubobjects(&raytracingPipeline);
  // This is a root signature that enables a shader to have unique arguments that come from shader tables.

  // Global root signature
  // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
  auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
  globalRootSignature->SetRootSignature(m_globalRootSignature.Get());

  auto pipelineConfigSubobject = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
  UINT maxRecursionDepth       = 1; // primary rays only.
  pipelineConfigSubobject->Config(maxRecursionDepth);

  throwIfFailed(getRTDevice()->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)));

  std::cout << "Created ray tracing pipeline" << std::endl;

#pragma region Old way of creating pipeline

  //// Ray Generation Shader
  // D3D12_DXIL_LIBRARY_DESC rayGenLibraryDesc     = {};
  // rayGenLibraryDesc.DXILLibrary.pShaderBytecode = rayGenShaderByteCode->GetBufferPointer();
  // rayGenLibraryDesc.DXILLibrary.BytecodeLength  = rayGenShaderByteCode->GetBufferSize();
  // D3D12_EXPORT_DESC rayGenExportDesc            = {L"MyRaygenShader", nullptr, D3D12_EXPORT_FLAG_NONE};
  // rayGenLibraryDesc.pExports                    = &rayGenExportDesc;
  // rayGenLibraryDesc.NumExports                  = 1;

  // D3D12_STATE_SUBOBJECT rayGenShaderObject = {};
  // rayGenShaderObject.Type                  = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
  // rayGenShaderObject.pDesc                 = &rayGenLibraryDesc;

  //// Miss Shader
  // D3D12_DXIL_LIBRARY_DESC missLibraryDesc     = {};
  // missLibraryDesc.DXILLibrary.pShaderBytecode = missShaderByteCode->GetBufferPointer();
  // missLibraryDesc.DXILLibrary.BytecodeLength  = missShaderByteCode->GetBufferSize();
  // D3D12_EXPORT_DESC missExportDesc            = {L"MyMissShader", nullptr, D3D12_EXPORT_FLAG_NONE};
  // missLibraryDesc.pExports                    = &missExportDesc;
  // missLibraryDesc.NumExports                  = 1;

  // D3D12_STATE_SUBOBJECT missShaderObject = {};
  // missShaderObject.Type                  = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
  // missShaderObject.pDesc                 = &missLibraryDesc;

  //// Closest Hit Shader
  // D3D12_DXIL_LIBRARY_DESC hitLibraryDesc     = {};
  // hitLibraryDesc.DXILLibrary.pShaderBytecode = hitShaderByteCode->GetBufferPointer();
  // hitLibraryDesc.DXILLibrary.BytecodeLength  = hitShaderByteCode->GetBufferSize();
  // D3D12_EXPORT_DESC hitExportDesc            = {L"MyClosestHitShader", nullptr, D3D12_EXPORT_FLAG_NONE};
  // hitLibraryDesc.pExports                    = &hitExportDesc;
  // hitLibraryDesc.NumExports                  = 1;

  // D3D12_STATE_SUBOBJECT hitShaderObject = {};
  // hitShaderObject.Type                  = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
  // hitShaderObject.pDesc                 = &hitLibraryDesc;

  //// create local root signatures
  // D3D12_STATE_SUBOBJECT localRootSigObject = {};
  // localRootSigObject.Type                  = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
  // localRootSigObject.pDesc                 = m_rootSignature.Get();

  //// Shader Configuration
  // D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
  // shaderConfig.MaxPayloadSizeInBytes          = 32; // Adjust as per your shader's ray payload size
  // shaderConfig.MaxAttributeSizeInBytes        = 8;  // Typical size for hit attributes

  // D3D12_STATE_SUBOBJECT shaderConfigSubobject = {};
  // shaderConfigSubobject.Type                  = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
  // shaderConfigSubobject.pDesc                 = &shaderConfig;

  //// pipeline config
  // D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
  // pipelineConfig.MaxTraceRecursionDepth           = 2; // Number of ray bounces
  // D3D12_STATE_SUBOBJECT pipelineConfigSubobject   = {};
  // pipelineConfigSubobject.Type                    = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
  // pipelineConfigSubobject.pDesc                   = &pipelineConfig;

  // D3D12_STATE_SUBOBJECT subobjects[] = {rayGenShaderObject,    missShaderObject,        hitShaderObject,
  //                                       shaderConfigSubobject, pipelineConfigSubobject, localRootSigObject};

  // D3D12_STATE_OBJECT_DESC rayTracingPipeline = {};
  // rayTracingPipeline.Type                    = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
  // rayTracingPipeline.NumSubobjects           = ARRAYSIZE(subobjects);
  // rayTracingPipeline.pSubobjects             = subobjects;

  // throwIfFailed(getRTDevice()->CreateStateObject(&rayTracingPipeline, IID_PPV_ARGS(&m_rtStateObject)));
#pragma endregion
}

void RayTracingRenderer::createShaderTables()
{
  auto device = getDevice();

  void* rayGenShaderIdentifier;
  void* missShaderIdentifier;
  void* hitGroupShaderIdentifier;

  auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
  {
    rayGenShaderIdentifier   = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);
    missShaderIdentifier     = stateObjectProperties->GetShaderIdentifier(c_missShaderName);
    hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_hitGroupName);
  };

  // Get shader identifiers.
  UINT shaderIdentifierSize;
  {
    ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
    throwIfFailed(m_dxrStateObject.As(&stateObjectProperties)); // first fix state object creation
    GetShaderIdentifiers(stateObjectProperties.Get());
    shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
  }

  // Ray gen shader table
  {
    struct RootArguments
    {
      RayGenConstantBuffer cb;
    } rootArguments;
    rootArguments.cb = m_rayGenCB;

    UINT        numShaderRecords = 1;
    UINT        shaderRecordSize = shaderIdentifierSize + sizeof(rootArguments);
    ShaderTable rayGenShaderTable(getDevice().Get(), numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
    rayGenShaderTable.push_back(
        ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize, &rootArguments, sizeof(rootArguments)));
    m_rayGenShaderTable = rayGenShaderTable.GetResource();
  }

  // Miss shader table
  {
    UINT        numShaderRecords = 1;
    UINT        shaderRecordSize = shaderIdentifierSize;
    ShaderTable missShaderTable(getDevice().Get(), numShaderRecords, shaderRecordSize, L"MissShaderTable");
    missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
    m_missShaderTable = missShaderTable.GetResource();
  }

  // Hit group shader table
  {
    UINT        numShaderRecords = 1;
    UINT        shaderRecordSize = shaderIdentifierSize;
    ShaderTable hitGroupShaderTable(getDevice().Get(), numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
    hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize));
    m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
  }
}

D3D12_RAYTRACING_GEOMETRY_DESC RayTracingRenderer::createGeometryDescription()
{
  D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
  geometryDesc.Type                           = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
  geometryDesc.Triangles.IndexBuffer          = m_indexBuffer->GetGPUVirtualAddress();
  geometryDesc.Triangles.IndexCount           = static_cast<UINT>(m_indexBuffer->GetDesc().Width) / sizeof(ui32);
  geometryDesc.Triangles.IndexFormat          = DXGI_FORMAT_R16_UINT;
  geometryDesc.Triangles.Transform3x4         = 0;
  geometryDesc.Triangles.VertexFormat         = DXGI_FORMAT_R32G32B32_FLOAT;
  geometryDesc.Triangles.VertexCount          = static_cast<UINT>(m_vertexBuffer->GetDesc().Width) / sizeof(Vertex);
  geometryDesc.Triangles.VertexBuffer.StartAddress  = m_vertexBuffer->GetGPUVirtualAddress();
  geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

  // Mark the geometry as opaque.
  // PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing
  // optimizations. Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is
  // present or not.
  geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

  return geometryDesc;
}

void RayTracingRenderer::createAccelerationStructures()
{
  // Reset the command list for the acceleration structure construction.
  getCommandList()->Reset(getCommandAllocator().Get(), nullptr);

  const auto geometryDescription = createGeometryDescription();

  // Get required sizes for an acceleration structure.
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
  topLevelInputs.DescsLayout                                          = D3D12_ELEMENTS_LAYOUT_ARRAY;
  topLevelInputs.Flags    = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  topLevelInputs.NumDescs = 1;
  topLevelInputs.Type     = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
  getRTDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
  throwIfZero(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS  bottomLevelInputs       = topLevelInputs;
  bottomLevelInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
  bottomLevelInputs.pGeometryDescs = &geometryDescription;
  getRTDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
  throwIfZero(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

  // Scratch resource used to
  ComPtr<ID3D12Resource> scratchResource;
  AllocateUAVBuffer(
      std::max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes),
      &scratchResource, D3D12_RESOURCE_STATE_COMMON, L"ScratchResource");

  // Allocate resources for acceleration structures.
  // Acceleration structures can only be placed in resources that are created in the default heap (or custom heap
  // equivalent). Default heap is OK since the application doesnt need CPU read/write access to them. The resources
  // that will contain acceleration structures must be created in the state
  // D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, and must have resource flag
  // D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both:
  //  - the system will be doing this type of access in its implementation of acceleration structure builds behind the
  //  scenes.
  //  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using
  // UAV /  barriers.
  {
    D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

    AllocateUAVBuffer(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_bottomLevelAS, initialResourceState,
                      L"BottomLevelAccelerationStructure");
    AllocateUAVBuffer(topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_topLevelAS, initialResourceState,
                      L"TopLevelAccelerationStructure");
  }

  // Create an instance desc for the bottom-level acceleration structure.
  ComPtr<ID3D12Resource>         instanceDescs;
  D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
  instanceDesc.Transform[0][0]                = 1.0f;
  instanceDesc.Transform[1][1]                = 1.0f;
  instanceDesc.Transform[2][2]                = 1.0f;
  instanceDesc.InstanceMask                   = 1;
  instanceDesc.AccelerationStructure          = m_bottomLevelAS->GetGPUVirtualAddress();
  AllocateUploadBuffer(&instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");

  // Bottom Level Acceleration Structure desc
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
  {
    bottomLevelBuildDesc.Inputs                           = bottomLevelInputs;
    bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
    bottomLevelBuildDesc.DestAccelerationStructureData    = m_bottomLevelAS->GetGPUVirtualAddress();
  }

  // Top Level Acceleration Structure desc
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
  {
    topLevelInputs.InstanceDescs                       = instanceDescs->GetGPUVirtualAddress();
    topLevelBuildDesc.Inputs                           = topLevelInputs;
    topLevelBuildDesc.DestAccelerationStructureData    = m_topLevelAS->GetGPUVirtualAddress();
    topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
  }

  auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
  {
    raytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
    auto uav = CD3DX12_RESOURCE_BARRIER::UAV(m_bottomLevelAS.Get());
    getCommandList()->ResourceBarrier(1, &uav);
    raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
  };

  // Build acceleration structure.
  BuildAccelerationStructure(m_dxrCommandLists[getFrameIndex()].Get());

  // Kick off acceleration structure construction.
  getCommandList()->Close(); // needs to be closed before execution
  ID3D12CommandList* commandLists[] = {getCommandList().Get()};
  getCommandQueue()->ExecuteCommandLists(1, commandLists);

  // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
  waitForGPU();
}

void RayTracingRenderer::createGeometry()
{
  // Create vertex buffer
  f32                 depthValue = 1.0f;
  f32                 offset     = 0.7f;
  std::vector<Vertex> vertexBuffer;
  vertexBuffer.reserve(3);
  f32v3 position1 = f32v3(0.0f, offset, depthValue);
  f32v3 position2 = f32v3(-offset, offset, depthValue);
  f32v3 position3 = f32v3(offset, offset, depthValue);
  vertexBuffer.emplace_back(position1);
  vertexBuffer.emplace_back(position2);
  vertexBuffer.emplace_back(position3);

  m_vertexBufferSize = static_cast<ui32>(vertexBuffer.size() * sizeof(Vertex));

  UploadHelper uploadHelper(getRTDevice(), m_vertexBufferSize);

  // Create resource on GPU
  const CD3DX12_RESOURCE_DESC   vertexBufferDescription = CD3DX12_RESOURCE_DESC::Buffer(m_vertexBufferSize);
  const CD3DX12_HEAP_PROPERTIES defaultHeapProperties   = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  getRTDevice()->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDescription,
                                         D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_vertexBuffer));

  // Upload to GPU
  uploadHelper.uploadBuffer(vertexBuffer.data(), m_vertexBuffer, m_vertexBufferSize, getCommandQueue());
  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes    = m_vertexBufferSize;
  m_vertexBufferView.StrideInBytes  = sizeof(Vertex);

  // Create index buffer
  std::vector<ui32> indices = {0, 1, 2};
  m_indexBufferSize         = static_cast<ui32>(indices.size() * sizeof(ui32));

  UploadHelper                uploadHelperIndexBuffer(getRTDevice(), m_indexBufferSize);
  const CD3DX12_RESOURCE_DESC indexBufferDescription = CD3DX12_RESOURCE_DESC::Buffer(m_indexBufferSize);
  getRTDevice()->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &indexBufferDescription,
                                         D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_indexBuffer));

  uploadHelperIndexBuffer.uploadBuffer(indices.data(), m_indexBuffer, m_indexBufferSize, getCommandQueue());
  m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
  m_indexBufferView.SizeInBytes    = m_indexBufferSize;
  m_indexBufferView.Format         = DXGI_FORMAT_R32_UINT;
}

void RayTracingRenderer::createDescriptorHeap()
{
  auto device = getDevice();

  D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
  // Allocate a heap for a single descriptor:
  // 1 - raytracing output texture UAV
  descriptorHeapDesc.NumDescriptors = 1;
  descriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  descriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  descriptorHeapDesc.NodeMask       = 0;
  device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
  NAME_D3D12_OBJECT(m_descriptorHeap);

  m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void RayTracingRenderer::createOutputResource()
{
  auto device = getDevice();

  D3D12_RESOURCE_DESC resourceDesc = {};
  resourceDesc.DepthOrArraySize    = 1;
  resourceDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  resourceDesc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  resourceDesc.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
  resourceDesc.Width               = getWidth();
  resourceDesc.Height              = getHeight();
  resourceDesc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  resourceDesc.MipLevels           = 1;
  resourceDesc.SampleDesc.Count    = 1;

  const auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_raytracingOutput));
  NAME_D3D12_OBJECT(m_raytracingOutput);

  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
  uavDesc.Format                           = DXGI_FORMAT_R8G8B8A8_UNORM;
  uavDesc.ViewDimension                    = D3D12_UAV_DIMENSION_TEXTURE2D;
  device->CreateUnorderedAccessView(m_raytracingOutput.Get(), nullptr, &uavDesc,
                                    m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
  m_raytracingOutputResourceUAVGpuDescriptor = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

#pragma endregion

#pragma region OnDraw

void RayTracingRenderer::onDraw()
{
  // throwIfFailed(m_dxrCommandList->Close());
  DoRayTracing();
  CopyRaytracingOutputToBackbuffer();
}

void RayTracingRenderer::onDrawUI()
{
}

void RayTracingRenderer::DoRayTracing()
{
  auto commandList      = getCommandList();
  auto commandAllocator = getCommandAllocator(); // Retrieve the command allocator

  // reset ray tracing specific command list
  // throwIfFailed(m_dxrCommandAllocator->Reset());
  // throwIfFailed(m_dxrCommandList->Reset(m_dxrCommandAllocator.Get(), nullptr));

  auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
  {
    // Since each shader table has only one shader record, the stride is same as the size.
    dispatchDesc->HitGroupTable.StartAddress             = m_hitGroupShaderTable->GetGPUVirtualAddress();
    dispatchDesc->HitGroupTable.SizeInBytes              = m_hitGroupShaderTable->GetDesc().Width;
    dispatchDesc->HitGroupTable.StrideInBytes            = dispatchDesc->HitGroupTable.SizeInBytes;
    dispatchDesc->MissShaderTable.StartAddress           = m_missShaderTable->GetGPUVirtualAddress();
    dispatchDesc->MissShaderTable.SizeInBytes            = m_missShaderTable->GetDesc().Width;
    dispatchDesc->MissShaderTable.StrideInBytes          = dispatchDesc->MissShaderTable.SizeInBytes;
    dispatchDesc->RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
    dispatchDesc->RayGenerationShaderRecord.SizeInBytes  = m_rayGenShaderTable->GetDesc().Width;
    dispatchDesc->Width                                  = getWidth();
    dispatchDesc->Height                                 = getHeight();
    dispatchDesc->Depth                                  = 1;
    commandList->SetPipelineState1(stateObject);
    commandList->DispatchRays(dispatchDesc);
  };

  commandList->SetComputeRootSignature(m_globalRootSignature.Get());

  // Bind the heaps, acceleration structure and dispatch rays.
  D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
  commandList->SetDescriptorHeaps(1, m_descriptorHeap.GetAddressOf());
  commandList->SetComputeRootDescriptorTable(0, m_raytracingOutputResourceUAVGpuDescriptor);
  commandList->SetComputeRootShaderResourceView(1, m_topLevelAS->GetGPUVirtualAddress());
  DispatchRays(m_dxrCommandLists[getFrameIndex()].Get(), m_dxrStateObject.Get(), &dispatchDesc);
  // throwIfFailed(m_dxrCommandList->Close());
  //// Submit the command list
  // ID3D12CommandList* commandLists[] = {m_dxrCommandList.Get()};
  // getCommandQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);
}

// Copy the raytracing output to the backbuffer.
void RayTracingRenderer::CopyRaytracingOutputToBackbuffer()
{
  auto commandList  = getCommandList();
  auto renderTarget = getRenderTarget().Get();

  D3D12_RESOURCE_BARRIER preCopyBarriers[2];

  // render target is copy destination
  preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                            D3D12_RESOURCE_STATE_COPY_DEST);

  // uav is copy source
  preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
      m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
  commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

  commandList->CopyResource(renderTarget, m_raytracingOutput.Get());

  D3D12_RESOURCE_BARRIER postCopyBarriers[2];
  postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST,
                                                             D3D12_RESOURCE_STATE_RENDER_TARGET);
  postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                                             D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

  commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
}

#pragma endregion

int main(int /* argc*/, char /* **argv */)
{
  gims::DX12AppConfig config;
  config.title    = L"Tutorial 17 RayTracing";
  config.useVSync = true;
  try
  {
    RayTracingRenderer renderer(config);
    renderer.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << "\n";
  }
  if (config.debug)
  {
    DX12Util::reportLiveObjects();
  }
  return 0;
}
