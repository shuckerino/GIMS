#pragma once

#include <gimslib/d3d/DX12App.hpp>
#include <gimslib/d3d/DX12Util.hpp>
#include <gimslib/d3d/UploadHelper.hpp>
#include <gimslib/dbg/HrException.hpp>
#include <gimslib/types.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

class RayTracingRenderer : public DX12App
{
public:
  RayTracingRenderer(const DX12AppConfig createInfo);

  // struct Vertex
  //{
  //   f32v3 position;
  // };
  struct Vertex
  {
    float v1, v2, v3;
  };

  /// <summary>
  /// Called whenever a new frame is drawn.
  /// </summary>
  virtual void onDraw();

  /// <summary>
  /// Draw UI onto of rendered result.
  /// </summary>
  virtual void onDrawUI();

  /// <summary>
  /// Adapt to new viewport
  /// </summary>
  virtual void onResize();

#pragma region Rasterizing

  ComPtr<ID3D12PipelineState> m_pipelineState;
  ComPtr<ID3D12RootSignature> m_rootSignature;

  void createRootSignature();
  void createPipeline();

#pragma endregion

private:
  // Acceleration structure
  ComPtr<ID3D12Resource> m_topLevelAS;
  ComPtr<ID3D12Resource> m_bottomLevelAS;

  // Root signatures
  ComPtr<ID3D12RootSignature> m_globalRootSignature;
  ComPtr<ID3D12RootSignature> m_LocalRootSignature;

  // Ray tracing output
  ComPtr<ID3D12Resource>      m_raytracingOutput;
  D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
  UINT                        m_raytracingOutputResourceUAVDescriptorHeapIndex;

  ComPtr<ID3D12StateObject> m_rtStateObject;

  // Descriptor heap
  ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
  ui32                         m_descriptorsAllocated;
  ui32                         m_descriptorSize;

  // Shader tables
  static const wchar_t*  c_hitGroupName;
  static const wchar_t*  c_raygenShaderName;
  static const wchar_t*  c_closestHitShaderName;
  static const wchar_t*  c_missShaderName;
  ComPtr<ID3D12Resource> m_missShaderTable;
  ComPtr<ID3D12Resource> m_hitGroupShaderTable;
  ComPtr<ID3D12Resource> m_rayGenShaderTable;

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
  typedef UINT16         Index;
  ui32                   m_vertexBufferSize; //! Vertex buffer size in bytes.
  ui32                   m_indexBufferSize;  //! Index buffer size in bytes.
  ComPtr<ID3D12Resource> m_indexBuffer;
  ComPtr<ID3D12Resource> m_vertexBuffer;

  // Init functions
  bool isRayTracingSupported();
  void createRayTracingResources();
  void createRootSignatures();
  void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);
  void createRayTracingPipeline();
  void createShaderTables();
  void createDescriptorHeap();

  /// <summary>
  /// Creates the ray tracing output resource (UAV)
  /// </summary>
  void createOutputResource();

  // Geometry functions
  void createGeometry();

  // Acceleration structure functions
  void                           createAccelerationStructures();
  D3D12_RAYTRACING_GEOMETRY_DESC createGeometryDescription();

  // Helper functions
  void AllocateUAVBuffer(ui64 bufferSize, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initialResourceState,
                         const wchar_t* resourceName);
  void DoRayTracing();

  /// <summary>
  /// Copies the raytracing output to the actual renderTarget
  /// </summary>
  void CopyRaytracingOutputToBackbuffer();
  struct AccelerationStructureBuffers
  {
    ComPtr<ID3D12Resource> pScratch;      // Scratch memory for AS builder
    ComPtr<ID3D12Resource> pResult;       // Where the AS is
    ComPtr<ID3D12Resource> pInstanceDesc; // Hold the matrices of the instances
  };
};
