#include "RayTracingRenderer.hpp"

using namespace gims;

#pragma region Helper functions

CD3DX12_SHADER_BYTECODE RayTracingRenderer::getShaderByteCode(const std::filesystem::path& shaderFile,
                                                              const wchar_t* entryPoint, const wchar_t* targetProfile)
{
  ComPtr<IDxcBlob> shaderBlob = compileShader(shaderFile, targetProfile, entryPoint);
  return CD3DX12_SHADER_BYTECODE(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
}

void RayTracingRenderer::AllocateUAVBuffer(ID3D12Device* device, UINT64 bufferSize, ID3D12Resource** ppResource,
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

  throwIfFailed(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &desc,
                                                initialResourceState, nullptr, IID_PPV_ARGS(ppResource)));
  (*ppResource)->SetName(resourceName);
}

#pragma endregion

#pragma region Init

RayTracingRenderer::RayTracingRenderer(const DX12AppConfig createInfo)
    : DX12App(createInfo)
{
  getDevice()->QueryInterface(IID_PPV_ARGS(&m_device));

  if (checkForRayTracingSupport() == false)
  {
    throw std::runtime_error("Ray tracing not supported on this device");
  }

  createRayTracingResources();
}

bool RayTracingRenderer::checkForRayTracingSupport()
{
  D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
  throwIfFailed(getRTDevice()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
  if (options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
  {
    std::cout << "Ray tracing not supported on this device" << std::endl;
  }
  else
  {
    std::cout << "Ray tracing supported on your device" << std::endl;
  }
}

/// <summary>
/// Initializing all resources needed for ray tracing
/// </summary>
void RayTracingRenderer::createRayTracingResources()
{
  createRayTracingInterfaces();
  createRootSignatures();
  createRayTracingPipeline();
  createShaderTables();
  createAccelerationStructures();
}

/// <summary>
/// Method for creating ID3D12Device5 and ID3D12GraphicsCommandList4 interfaces
/// </summary>
void RayTracingRenderer::createRayTracingInterfaces()
{
  throwIfFailed(getDevice()->QueryInterface(IID_PPV_ARGS(&m_device)));
  throwIfFailed(getDevice()->QueryInterface(IID_PPV_ARGS(&m_dxrCommandList)));
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
    ComPtr<ID3DBlob>            rootBlob, errorBlob;
    D3D12SerializeRootSignature(&globalRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob);

    getDevice()->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(),
                                     IID_PPV_ARGS(&m_globalRootSignature));
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
  }

  //CD3DX12_DESCRIPTOR_RANGE srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 for Scene
  //CD3DX12_DESCRIPTOR_RANGE uavRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0 for RenderTarget

  //CD3DX12_ROOT_PARAMETER params[3] = {};
  //params[0].InitAsDescriptorTable(1, &srvRange);                      // SRV table
  //params[1].InitAsDescriptorTable(1, &uavRange);                      // UAV table
  //params[2].InitAsConstantBufferView(0, D3D12_SHADER_VISIBILITY_ALL); // b0 for g_rayGenCB

  //CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
  //rootSignatureDesc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

  //ComPtr<ID3DBlob> serializedRootSig, errorBlob;

  //HRESULT hr =
  //    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
  //if (FAILED(hr))
  //{
  //  OutputDebugStringA((char*)errorBlob->GetBufferPointer());
  //  throw std::runtime_error("Failed to serialize root signature.");
  //}

  //hr = getRTDevice()->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
  //                                        IID_PPV_ARGS(&m_globalRootSignature));

  //if (FAILED(hr))
  //{
  //  throw std::runtime_error("Failed to create root signature.");
  //}
}

void RayTracingRenderer::createRayTracingPipeline()
{
  CD3DX12_SHADER_BYTECODE rayGenShaderByteCode = getShaderByteCode(
      L"../../../Tutorials/T17TriangleRayTracing/Shaders/Triangle.hlsl", L"MyRaygenShader", L"lib_6_3");
  CD3DX12_SHADER_BYTECODE missShaderByteCode =
      getShaderByteCode(L"../../../Tutorials/T17TriangleRayTracing/Shaders/Triangle.hlsl", L"MyMissShader", L"lib_6_3");
  CD3DX12_SHADER_BYTECODE hitShaderByteCode = getShaderByteCode(
      L"../../../Tutorials/T17TriangleRayTracing/Shaders/Triangle.hlsl", L"MyClosestHitShader", L"lib_6_3");

  CD3DX12_STATE_OBJECT_DESC raytracingPipeline {D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE};

  // DXIL Library Subobject for RayGen
  CD3DX12_DXIL_LIBRARY_SUBOBJECT rayGenLibrary = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
  rayGenLibrary.SetDXILLibrary(&rayGenShaderByteCode);
  rayGenLibrary.DefineExport(L"MyRaygenShader");

  // DXIL Library Subobject for Miss
  CD3DX12_DXIL_LIBRARY_SUBOBJECT missLibrary = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
  missLibrary.SetDXILLibrary(&missShaderByteCode);
  missLibrary.DefineExport(L"MyMissShader");

  // DXIL Library Subobject for Hit
  CD3DX12_DXIL_LIBRARY_SUBOBJECT hitLibrary = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
  hitLibrary.SetDXILLibrary(&hitShaderByteCode);
  hitLibrary.DefineExport(L"MyClosestHitShader");

  // Define Shader Config
  auto shaderConfig  = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
  UINT payloadSize   = 1 * sizeof(float); // float rayHitT
  UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
  shaderConfig->Config(payloadSize, attributeSize);

  // Pipeline Configuration
  D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
  pipelineConfig.MaxTraceRecursionDepth           = 2; // Example value

  CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT pipelineConfigSubobject(pipelineConfig);

  throwIfFailed(getRTDevice()->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)));

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
}

void RayTracingRenderer::createAccelerationStructures()
{
  //// Reset the command list for the acceleration structure construction.
  // getCommandList()->Reset(getCommandAllocator(), nullptr);

  // D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
  // geometryDesc.Type                           = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
  // geometryDesc.Triangles.IndexBuffer          = m_indexBuffer->GetGPUVirtualAddress();
  // geometryDesc.Triangles.IndexCount           = static_cast<UINT>(m_indexBuffer->GetDesc().Width) / sizeof(Index);
  // geometryDesc.Triangles.IndexFormat          = DXGI_FORMAT_R16_UINT;
  // geometryDesc.Triangles.Transform3x4         = 0;
  // geometryDesc.Triangles.VertexFormat         = DXGI_FORMAT_R32G32B32_FLOAT;
  // geometryDesc.Triangles.VertexCount          = static_cast<UINT>(m_vertexBuffer->GetDesc().Width) / sizeof(Vertex);
  // geometryDesc.Triangles.VertexBuffer.StartAddress  = m_vertexBuffer->GetGPUVirtualAddress();
  // geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

  //// Mark the geometry as opaque.
  //// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing
  //// optimizations. Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is
  //// present or not.
  // geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

  //// Get required sizes for an acceleration structure.
  // D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags =
  //     D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  // D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
  // topLevelInputs.DescsLayout                                          = D3D12_ELEMENTS_LAYOUT_ARRAY;
  // topLevelInputs.Flags                                                = buildFlags;
  // topLevelInputs.NumDescs                                             = 1;
  // topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

  // D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
  // getRTDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
  // throwIfZero(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

  // D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
  // D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS  bottomLevelInputs       = topLevelInputs;
  // bottomLevelInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
  // bottomLevelInputs.pGeometryDescs = &geometryDesc;
  // getRTDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
  // throwIfZero(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

  // ComPtr<ID3D12Resource> scratchResource;
  // AllocateUAVBuffer(
  //     getRTDevice(),
  //     std::max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes),
  //     &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

  //// Allocate resources for acceleration structures.
  //// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap
  //// equivalent). Default heap is OK since the application doesnt need CPU read/write access to them. The resources
  //// that will contain acceleration structures must be created in the state
  //// D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, and must have resource flag
  //// D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both:
  ////  - the system will be doing this type of access in its implementation of acceleration structure builds behind the
  ////  scenes.
  ////  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using
  ///UAV /  barriers.
  //{
  //  D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

  //  AllocateUAVBuffer(getRTDevice(), bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_bottomLevelAS,
  //                    initialResourceState, L"BottomLevelAccelerationStructure");
  //  AllocateUAVBuffer(getRTDevice(), topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_topLevelAS,
  //  initialResourceState,
  //                    L"TopLevelAccelerationStructure");
  //}

  //// Create an instance desc for the bottom-level acceleration structure.
  // ComPtr<ID3D12Resource>         instanceDescs;
  // D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
  // instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
  // instanceDesc.InstanceMask                                                                  = 1;
  // instanceDesc.AccelerationStructure = m_bottomLevelAS->GetGPUVirtualAddress();
  // AllocateUploadBuffer(getRTDevice(), &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");

  //// Bottom Level Acceleration Structure desc
  // D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
  //{
  //   bottomLevelBuildDesc.Inputs                           = bottomLevelInputs;
  //   bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
  //   bottomLevelBuildDesc.DestAccelerationStructureData    = m_bottomLevelAS->GetGPUVirtualAddress();
  // }

  //// Top Level Acceleration Structure desc
  // D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
  //{
  //   topLevelInputs.InstanceDescs                       = instanceDescs->GetGPUVirtualAddress();
  //   topLevelBuildDesc.Inputs                           = topLevelInputs;
  //   topLevelBuildDesc.DestAccelerationStructureData    = m_topLevelAS->GetGPUVirtualAddress();
  //   topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
  // }

  // auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
  //{
  //   raytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
  //   getCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_bottomLevelAS.Get()));
  //   raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
  // };

  //// Build acceleration structure.
  // BuildAccelerationStructure(m_dxrCommandList.Get());

  //// Kick off acceleration structure construction.
  // m_deviceResources->ExecuteCommandList();

  //// Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
  // m_deviceResources->WaitForGpu();
}
#pragma endregion

#pragma region OnRender

void RayTracingRenderer::DoRayTracing()
{
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
