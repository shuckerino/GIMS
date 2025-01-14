#pragma once

#include <gimslib/d3d/DX12App.hpp>
#include <gimslib/d3d/DX12Util.hpp>
#include <gimslib/d3d/UploadHelper.hpp>
#include <gimslib/dbg/HrException.hpp>
#include <gimslib/types.hpp>
#include <gimslib/ui/ExaminerController.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

class RayTracingRenderer : public DX12App
{
public:
  RayTracingRenderer(const DX12AppConfig createInfo);

  struct Vertex
  {
    float v1, v2, v3;
  };

  struct InstanceData
  {
    f32m4 worldMatrix;
  };

  struct UiData
  {
    f32v3 m_lightDirection;
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

  void createPipeline();

#pragma endregion

private:
  // Camera
  gims::ExaminerController m_examinerController;

  // Acceleration structure
  ComPtr<ID3D12Resource>              m_topLevelAS;
  std::vector<ComPtr<ID3D12Resource>> m_bottomLevelAS;

  // Root signatures
  ComPtr<ID3D12RootSignature> m_globalRootSignature;

  // Ui data
  UiData m_uiData;

  // Geometry
  typedef ui32             Index;
  ui32                     m_vertexBufferSize;        //! Vertex buffer size in bytes.
  ui32                     m_indexBufferSize;         //! Index buffer size in bytes.
  ui32                     m_numTriangleIndices;      //! Num indices for triangle
  InstanceData             m_triangleInstanceData[3]; //! Instance data for triangle
  ComPtr<ID3D12Resource>   m_triangleIndexBuffer;
  ComPtr<ID3D12Resource>   m_triangleVertexBuffer;
  ComPtr<ID3D12Resource>   m_instanceBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView;
  D3D12_VERTEX_BUFFER_VIEW m_triangleVertexBufferView;
  D3D12_INDEX_BUFFER_VIEW  m_triangleIndexBufferView;

  // plane
  ui32                     m_numPlaneIndices; //! Num indices for plane
  ComPtr<ID3D12Resource>   m_planeVertexBuffer;
  ComPtr<ID3D12Resource>   m_planeIndexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_planeVertexBufferView;
  D3D12_INDEX_BUFFER_VIEW  m_planeIndexBufferView;

  // Init functions
  bool isRayTracingSupported();
  void createRayTracingResources();
  void createRootSignature();

  // Geometry functions
  void createGeometry();

  // Acceleration structure functions
  void                           createAccelerationStructures();

  // Helper functions
  void AllocateUAVBuffer(ui64 bufferSize, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initialResourceState,
                         const wchar_t* resourceName);
};
