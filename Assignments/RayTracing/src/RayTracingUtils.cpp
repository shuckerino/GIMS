﻿#include "RayTracingUtils.hpp"
#include "SceneGraphViewerApp.hpp" // Full definition needed here

namespace
{
// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
  pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
  WCHAR fullName[50];
  if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
  {
    pObject->SetName(fullName);
  }
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
#endif

// Naming helper for ComPtr<T>.
// Assigns the name of the variable as the name of the object.
// The indexed variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x)            SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

#pragma region Helper functions

// Check if can be replaced with UploadHelper
inline void AllocateUploadBuffer(ID3D12Device* pDevice, void* pData, UINT64 datasize, ID3D12Resource** ppResource,
                                 const wchar_t* resourceName = nullptr)
{
  auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto bufferDesc           = CD3DX12_RESOURCE_DESC::Buffer(datasize);
  throwIfFailed(pDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(ppResource)));
  if (resourceName)
  {
    (*ppResource)->SetName(resourceName);
  }
  void* pMappedData;
  (*ppResource)->Map(0, nullptr, &pMappedData);
  memcpy(pMappedData, pData, datasize);
  (*ppResource)->Unmap(0, nullptr);
}

void AllocateUAVBuffer(ComPtr<ID3D12Device5> device, ui64 bufferSize, ID3D12Resource** ppResource,
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

} // namespace

#pragma region Init

RayTracingUtils RayTracingUtils::createRayTracingUtils(ComPtr<ID3D12Device5> device, Scene& scene,
                                                       ComPtr<ID3D12GraphicsCommandList4> commandList,
                                                       ComPtr<ID3D12CommandAllocator>     commandAllocator,
                                                       ComPtr<ID3D12CommandQueue> commandQueue, ui32 vp_height,
                                                       ui32 vp_width, SceneGraphViewerApp& app)
{
  RayTracingUtils rayTracingUtils(device);

  // rayTracingUtils.createOutputResource(device, vp_width, vp_height);
  (void)vp_height;
  (void)vp_width;
  rayTracingUtils.createAccelerationStructures(device, scene, commandList, commandAllocator, commandQueue, app);

  return rayTracingUtils;
}

RayTracingUtils::RayTracingUtils(ComPtr<ID3D12Device5> device)
{
  if (isRayTracingSupported(device) == false)
  {
    throw std::runtime_error("Ray tracing not supported on this device");
  }
}

bool RayTracingUtils::isRayTracingSupported(ComPtr<ID3D12Device5> device)
{
  D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
  throwIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
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

#pragma region Build acceleration structures

/// <summary>
/// Building geometry description for each mesh in the scene
/// </summary>
/// <param name="geometryDescriptions"></param>
/// <param name="scene"></param>
void RayTracingUtils::buildGeometryDescriptionsForBLAS(
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDescriptions, Scene& scene)
{
  D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

  for (ui16 i = 0; i < scene.getNumberOfMeshes(); i++)
  {
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type                           = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.IndexBuffer          = scene.getMesh(i).getIndexBuffer()->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexCount =
        static_cast<ui32>(scene.getMesh(i).getIndexBuffer()->GetDesc().Width) / sizeof(ui32);
    geometryDesc.Triangles.IndexFormat  = DXGI_FORMAT_R32_UINT;
    geometryDesc.Triangles.Transform3x4 = 0;
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.VertexCount =
        static_cast<ui32>(scene.getMesh(i).getVertexBuffer()->GetDesc().Width) / sizeof(Vertex);
    geometryDesc.Triangles.VertexBuffer.StartAddress  = scene.getMesh(i).getVertexBuffer()->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    geometryDesc.Flags                                = geometryFlags;

    geometryDescriptions.push_back(geometryDesc);
  }
}

AccelerationStructureBuffers RayTracingUtils::buildBottomLevelAS(
    ComPtr<ID3D12Device5> device, ComPtr<ID3D12GraphicsCommandList4> commandList,
    const D3D12_RAYTRACING_GEOMETRY_DESC& geometryDescription)
{
  ComPtr<ID3D12Resource> scratchResource;
  ComPtr<ID3D12Resource> bottomLevelAS;

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS  bottomLevelInputs       = {};
  bottomLevelInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
  bottomLevelInputs.pGeometryDescs = &geometryDescription;
  device->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
  throwIfZero(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

  // Create scratch buffer
  AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &scratchResource,
                    D3D12_RESOURCE_STATE_COMMON, L"BLAS_ScratchResource");

  AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_bottomLevelAS,
                    D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"BottomLevelAccelerationStructure");

  // Bottom Level Acceleration Structure desc
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
  {
    bottomLevelBuildDesc.Inputs                           = bottomLevelInputs;
    bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
    bottomLevelBuildDesc.DestAccelerationStructureData    = m_bottomLevelAS->GetGPUVirtualAddress();
  }

  commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

  AccelerationStructureBuffers bottomLevelASBuffers;
  bottomLevelASBuffers.accelerationStructure    = bottomLevelAS;
  bottomLevelASBuffers.scratch                  = scratchResource;
  bottomLevelASBuffers.ResultDataMaxSizeInBytes = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;
  return bottomLevelASBuffers;
}

AccelerationStructureBuffers RayTracingUtils::buildTopLevelAS(ComPtr<ID3D12Device5>                     device,
                                                              ComPtr<ID3D12GraphicsCommandList4>        commandList,
                                                              std::vector<AccelerationStructureBuffers> bottomLevelAS)
{
  ComPtr<ID3D12Resource> scratchResource;
  ComPtr<ID3D12Resource> topLevelAS;

  // Get required sizes for an acceleration structure.
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs    = {};
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC   topLevelBuildDesc = {};
  topLevelInputs.DescsLayout                                             = D3D12_ELEMENTS_LAYOUT_ARRAY;
  topLevelInputs.Flags    = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  topLevelInputs.NumDescs = 1; // only triangle geometry
  topLevelInputs.Type     = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
  device->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
  throwIfZero(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

  // Create scratch buffer
  AllocateUAVBuffer(device, topLevelPrebuildInfo.ScratchDataSizeInBytes, &scratchResource, D3D12_RESOURCE_STATE_COMMON,
                    L"ScratchResource");

  D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

  AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_topLevelAS, initialResourceState,
                    L"TopLevelAccelerationStructure");

  D3D12_GPU_VIRTUAL_ADDRESS bottomLevelASaddresses = {
      bottomLevelAS.at(0).accelerationStructure->GetGPUVirtualAddress(),
  };
  BuildBotomLevelASInstanceDescs<D3D12_RAYTRACING_INSTANCE_DESC>(bottomLevelASaddresses, &instanceDescsResource);

  instanceDesc.Transform[0][0]       = 1.0f;
  instanceDesc.Transform[1][1]       = 1.0f;
  instanceDesc.Transform[2][2]       = 1.0f;
  instanceDesc.InstanceMask          = 1;
  instanceDesc.AccelerationStructure = m_bottomLevelAS->GetGPUVirtualAddress();

  AllocateUploadBuffer(device.Get(), &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");

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
}

#pragma endregion

void RayTracingUtils::createAccelerationStructures(ComPtr<ID3D12Device5> device, Scene& scene,
                                                   ComPtr<ID3D12GraphicsCommandList4> commandList,
                                                   ComPtr<ID3D12CommandAllocator>     commandAllocator,
                                                   ComPtr<ID3D12CommandQueue> commandQueue, SceneGraphViewerApp& app)
{

  // Reset the command list for the acceleration structure construction.
  commandList->Reset(commandAllocator.Get(), nullptr);
  const ui16 numMeshes = scene.getNumberOfMeshes();

  // Build all BLAS for scene
  std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescriptions;
  geometryDescriptions.reserve(numMeshes);
  buildGeometryDescriptionsForBLAS(geometryDescriptions, scene);

  std::vector<AccelerationStructureBuffers>   bottomLevelAccelerationStructures;
  std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;
  bottomLevelAccelerationStructures.reserve(numMeshes);
  instanceDescs.resize(numMeshes);
  for (ui16 i = 0; i < geometryDescriptions.size(); i++)
  {
    bottomLevelAccelerationStructures.push_back(buildBottomLevelAS(device, commandList, geometryDescriptions.at(i)));

    // create instance description for each BLAS
    instanceDescs[i].Transform[0][0]       = 1.0f;
    instanceDescs[i].Transform[1][1]       = 1.0f;
    instanceDescs[i].Transform[2][2]       = 1.0f;
    instanceDescs[i].InstanceMask          = 1;
    instanceDescs[i].AccelerationStructure = m_bottomLevelAS->GetGPUVirtualAddress();
  }

  ComPtr<ID3D12Resource> instanceDescsBuffer;
  AllocateUploadBuffer(device.Get(), instanceDescs.data(), sizeof(instanceDescs), &instanceDescsBuffer,
                       L"InstanceDescs");

  // build TLAS
  AccelerationStructureBuffers topLevelAS = buildTopLevelAS(device, commandList, bottomLevelAccelerationStructures);

  auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
  {
    raytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
    auto uav = CD3DX12_RESOURCE_BARRIER::UAV(m_bottomLevelAS.Get());
    raytracingCommandList->ResourceBarrier(1, &uav);
    raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
  };

  // Build acceleration structure.
  BuildAccelerationStructure(commandList.Get());

  // Start acceleration structure construction
  commandList->Close(); // needs to be closed before execution
  ID3D12CommandList* commandLists[] = {commandList.Get()};
  commandQueue->ExecuteCommandLists(1, commandLists);

  // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
  app.waitForGPU();
}

#pragma endregion