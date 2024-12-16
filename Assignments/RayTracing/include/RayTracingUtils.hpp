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
#include <SceneGraphViewerApp.hpp>

class RayTracingUtils
{
public:
  RayTracingUtils(ComPtr<ID3D12Device5> device);
  inline ~RayTracingUtils();

  static RayTracingUtils createRayTracingUtils(ComPtr<ID3D12Device5> device, Scene& scene,
                                               ComPtr<ID3D12GraphicsCommandList4> commandList,
                                               ComPtr<ID3D12CommandAllocator>     commandAllocator,
                                               ComPtr<ID3D12CommandQueue> commandQueue,
                                               ui32 vp_height, ui32 vp_width, SceneGraphViewerApp& app);

private:
  // Acceleration structure
  ComPtr<ID3D12Resource> m_topLevelAS;
  ComPtr<ID3D12Resource> m_bottomLevelAS;

  // Ray tracing output
  ComPtr<ID3D12Resource>      m_raytracingOutput;
  D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
  UINT                        m_raytracingOutputResourceUAVDescriptorHeapIndex;

  // Descriptor heap
  ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
  ui32                         m_descriptorsAllocated;
  ui32                         m_descriptorSize;

  bool isRayTracingSupported(ComPtr<ID3D12Device5> device);
  void createDescriptorHeap(ComPtr<ID3D12Device5> device);
  void createOutputResource(ComPtr<ID3D12Device5> device, ui32 width, ui32 height);
  void createAccelerationStructures(ComPtr<ID3D12Device5> device, Scene& scene,
                                    ComPtr<ID3D12GraphicsCommandList4> commandList,
                                    ComPtr<ID3D12CommandAllocator>     commandAllocator,
                                    ComPtr<ID3D12CommandQueue> commandQueue, SceneGraphViewerApp& app);
};

RayTracingUtils::~RayTracingUtils()
{
}
