#pragma once
#include <gimslib/d3d/DX12App.hpp>
#include <gimslib/io/CograBinaryMeshFile.hpp>
#include <gimslib/types.hpp>
#include <gimslib/ui/ExaminerController.hpp>

using namespace gims;
class MeshViewer : public gims::DX12App
{
public:
  MeshViewer(const DX12AppConfig config);

  ~MeshViewer();

  virtual void onDraw();
  virtual void onDrawUI();

private:
  struct UiData
  {
    f32v3 m_backgroundColor = f32v3(0.25f, 0.25f, 0.25f);
    f32   m_width;
    f32   m_height;
    bool  wireFrameEnabled;
    // TODO Implement me!
  };
  struct Vertex
  {
    f32v3 position;
    //f32v3 normal;   // Normal vector
    //f32v2 texcoord; // Texture coordinates
  };
  struct ConstantBuffer
  {
    f32m4 mvp;
    //f32m4 mv;
    //f32v4 specularColor_and_Exponent;
    //f32v4 ambientColor;
    //f32v4 diffuseColor;
    //f32v4 wireFrameColor;
    //ui32  flags;
  };

  gims::ExaminerController            m_examinerController;
  f32m4                               m_normalizationTransformation;
  std::vector<Vertex>                 m_VertexBufferCPU;
  D3D12_VERTEX_BUFFER_VIEW            m_vertexBufferView;
  size_t                              m_vertexBufferSize;
  ComPtr<ID3D12Resource>              m_vertexBufferGPU;

  ComPtr<ID3D12RootSignature>         m_rootSignature;

  ComPtr<ID3D12PipelineState>         m_pipelineState;
  ComPtr<ID3D12PipelineState>         m_wireFramePipelineState;

  std::vector<ComPtr<ID3D12Resource>> m_constantBuffers;

  std::vector<ui32>                   m_indexBuffer;
  ComPtr<ID3D12Resource>              m_indexBufferGPU;
  size_t                              m_indexBufferSize;
  D3D12_INDEX_BUFFER_VIEW             m_indexBufferView;

  UiData                              m_uiData;

  void createRootSignature();
  void createConstantBufferForEachSwapchainFrame();
  void updateConstantBuffer();
  void createPipeline();
  void createWireFramePipeline();
  void initializeVertexBuffer(const CograBinaryMeshFile* cbm);
  void uploadVertexBufferToGPU();
  void initializeIndexBuffer(const CograBinaryMeshFile* cbm);
  void uploadIndexBufferToGPU();
};
