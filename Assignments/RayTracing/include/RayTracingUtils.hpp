#pragma once

#include <Scene.hpp>
#include <TriangleMeshD3D12.hpp>
#include <gimslib/d3d/DX12App.hpp>
#include <gimslib/d3d/DX12Util.hpp>
#include <gimslib/d3d/UploadHelper.hpp>
#include <gimslib/dbg/HrException.hpp>
#include <gimslib/types.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

class SceneGraphViewerApp; // Forward declaration

struct AccelerationStructureBuffers
{
  ComPtr<ID3D12Resource> scratch;
  ComPtr<ID3D12Resource> accelerationStructure;
  ComPtr<ID3D12Resource> instanceDesc; // Used only for top-level AS
  UINT64                 ResultDataMaxSizeInBytes;
};

class RayTracingUtils
{
public:
  RayTracingUtils(ComPtr<ID3D12Device5> device);
  inline ~RayTracingUtils();

  static RayTracingUtils createRayTracingUtils(ComPtr<ID3D12Device5> device, Scene& scene,
                                               ComPtr<ID3D12GraphicsCommandList4> commandList,
                                               ComPtr<ID3D12CommandAllocator>     commandAllocator,
                                               ComPtr<ID3D12CommandQueue> commandQueue, ui32 vp_height, ui32 vp_width,
                                               SceneGraphViewerApp& app);

  // Acceleration structure
  ComPtr<ID3D12Resource> m_topLevelAS;
  ComPtr<ID3D12Resource> m_bottomLevelAS;

  bool isRayTracingSupported(ComPtr<ID3D12Device5> device);
  void createAccelerationStructures(ComPtr<ID3D12Device5> device, Scene& scene,
                                    ComPtr<ID3D12GraphicsCommandList4> commandList,
                                    ComPtr<ID3D12CommandAllocator>     commandAllocator,
                                    ComPtr<ID3D12CommandQueue> commandQueue, SceneGraphViewerApp& app);

  void buildGeometryDescriptionsForBLAS(std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDescriptions,
                                        Scene&                                       scene);
  AccelerationStructureBuffers buildBottomLevelAS(ComPtr<ID3D12Device5>                 device,
                                                  ComPtr<ID3D12GraphicsCommandList4>    commandList,
                                                  const D3D12_RAYTRACING_GEOMETRY_DESC& geometryDescription);

  AccelerationStructureBuffers buildTopLevelAS(
      ComPtr<ID3D12Device5> device, ComPtr<ID3D12GraphicsCommandList4> commandList,
      std::vector<AccelerationStructureBuffers> bottomLevelAS);
};

RayTracingUtils::~RayTracingUtils()
{
}
