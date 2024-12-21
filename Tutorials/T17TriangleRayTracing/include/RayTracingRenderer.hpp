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

  // Root signatures
  ComPtr<ID3D12RootSignature> m_globalRootSignature;

  // Geometry
  typedef UINT16         Index;
  ui32                   m_vertexBufferSize; //! Vertex buffer size in bytes.
  ui32                   m_indexBufferSize;  //! Index buffer size in bytes.
  ui32                   m_numIndices;  //! Num indices
  ComPtr<ID3D12Resource> m_indexBuffer;
  ComPtr<ID3D12Resource> m_vertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
  D3D12_INDEX_BUFFER_VIEW  m_indexBufferView;

  // Init functions
  bool isRayTracingSupported();
  void createRayTracingResources();
  void createRootSignatures();
 
  // Geometry functions
  void createGeometry();

  // Acceleration structure functions
  void                           createAccelerationStructures();
  D3D12_RAYTRACING_GEOMETRY_DESC createGeometryDescription();

  // Helper functions
  void AllocateUAVBuffer(ui64 bufferSize, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initialResourceState,
                         const wchar_t* resourceName);
 
};
