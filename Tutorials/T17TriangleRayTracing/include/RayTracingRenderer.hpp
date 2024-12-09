#pragma once
#include <gimslib/d3d/DX12App.hpp>
#include <gimslib/d3d/DX12Util.hpp>
#include <gimslib/dbg/HrException.hpp>
#include <gimslib/types.hpp>
#include <gimslib/d3d/UploadHelper.hpp>
#include <iostream>

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

class RayTracingRenderer : public DX12App
{
public:
  RayTracingRenderer(const DX12AppConfig createInfo);

  struct Vertex
  {
    f32v3 position;
  };

  ID3D12Device5* getRTDevice()
  {
    return m_device.Get();
  }

private:
  // DirectX ray tracing interfaces
  ComPtr<ID3D12Device5>              m_device;
  ComPtr<ID3D12GraphicsCommandList4> m_dxrCommandList;
  ComPtr<ID3D12StateObject>          m_dxrStateObject;

  // Acceleration structure
  ComPtr<ID3D12Resource> m_topLevelAS;
  ComPtr<ID3D12Resource> m_bottomLevelAS;

  // Root signatures
  ComPtr<ID3D12Resource>      m_globalRootSignature;
  ComPtr<ID3D12RootSignature> m_LocalRootSignature;

  // Ray tracing output
  ComPtr<ID3D12Resource>       m_outputResource;
  ComPtr<ID3D12Resource>       m_shaderTable;
  ComPtr<ID3D12StateObject>    m_rtStateObject;
  ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
  ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
  ComPtr<ID3D12DescriptorHeap> m_rayGenHeap;
  ComPtr<ID3D12DescriptorHeap> m_missHeap;
  ComPtr<ID3D12DescriptorHeap> m_hitHeap;
  ComPtr<ID3D12DescriptorHeap> m_outputHeap;
  ComPtr<ID3D12DescriptorHeap> m_shaderTableHeap;

  // Raytracing scene
  struct Viewport
  {
    float left;
    float top;
    float right;
    float bottom;
  };
  struct RayGenConstantBuffer
  {
    Viewport viewport;
    Viewport stencil;
  };
  RayGenConstantBuffer m_rayGenCB;

  // Geometry
  ui32                     m_vertexBufferSize; //! Vertex buffer size in bytes.
  ui32                     m_indexBufferSize;  //! Index buffer size in bytes.
  ComPtr<ID3D12Resource> m_indexBuffer;
  ComPtr<ID3D12Resource> m_vertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
  D3D12_INDEX_BUFFER_VIEW  m_indexBufferView;

  // Init functions
  void createRayTracingResources();
  void createRayTracingInterfaces();
  bool rayTracingSupported();
  void createRootSignatures();
  void createRayTracingPipeline();
  void createShaderTables();

  // Geometry functions
  void createGeometry();

  // Acceleration structure functions
  void createAccelerationStructures();
  void createBottomLevelAS();
  void createTopLevelAS();

  // Helper functions
  void AllocateUAVBuffer(ID3D12Device* device, UINT64 bufferSize, ID3D12Resource** ppResource,
                         D3D12_RESOURCE_STATES initialResourceState, const wchar_t* resourceName);
  void DoRayTracing();
};
