#include "TriangleApp.h"
#include <d3dx12/d3dx12.h>
#include <gimslib/contrib/stb/stb_image.h>
#include <gimslib/d3d/DX12Util.hpp>
#include <gimslib/d3d/UploadHelper.hpp>
#include <gimslib/dbg/HrException.hpp>
#include <gimslib/sys/Event.hpp>
#include <imgui.h>
#include <iostream>
#include <vector>

using namespace gims;

namespace
{
f32m4 getNormalizationTransformation(f32v3 const* const positions, ui32 nPositions)
{
  if (nPositions == 0)
    return f32m4(1);

  f32v3 minPos = positions[0];
  f32v3 maxPos = positions[0];

  // We need to translate the vertices to the center of the cuboid
  // Therefore we first need to calculate the center of mass of the vertices
  f32v3 centerOfMass(0.0f);
  for (ui32 i = 0; i < nPositions; i++)
  {
    minPos = glm::min(minPos, positions[i]);
    maxPos = glm::max(minPos, positions[i]);
    centerOfMass += positions[i];
  }

  // cast to float to avoid integer division
  centerOfMass /= static_cast<float>(nPositions);
  f32m4 translation = glm::translate(glm::mat4(1.0f), -centerOfMass);

  // find largest dimension and scale accordingly
  const f32v3 distance      = maxPos - minPos;
  const f32        largestDimension = glm::max(distance.x, glm::max(distance.y, distance.z));
  const f32 scalingFactor    = (largestDimension != 0) ? (1.0f / largestDimension) : 1.0f;
  f32m4 scale_matrix = glm::scale(glm::mat4(1.0f), f32v3(scalingFactor));

  // Combine translation and scaling
  f32m4 normalizationMatrix = scale_matrix * translation;

  return normalizationMatrix;
}
} // namespace

MeshViewer::MeshViewer(const DX12AppConfig config)
    : DX12App(config)
    , m_examinerController(true)
{
  // set ui data
  m_uiData.m_viewPortHeight   = static_cast<f32>(config.height);
  m_uiData.m_viewPortWidth    = static_cast<f32>(config.width);
  m_uiData.m_backgroundColor  = f32v4(1.f, 1.f, 1.f, 1.0f);
  m_uiData.m_wireFrameColor   = f32v4(0.0f, 0.f, 0.0f, 1.0f);
  m_uiData.m_wireFrameEnabled = true;

  createRootSignature();
  createPipeline();
  createWireFramePipeline();
  createConstantBufferForEachSwapchainFrame();
  m_examinerController.setTranslationVector(f32v3(0, 0, 3));

  // Load the model data from file
  CograBinaryMeshFile cbm("../../../data/bunny.cbm");

  // Test print some stats
  cbm.printAttributes(std::cout);
  cbm.printConstant(std::cout);
  std::cout << cbm.getNumTriangles();

  initializeVertexBuffer(&cbm);
  uploadVertexBufferToGPU();

  initializeIndexBuffer(&cbm);
  uploadIndexBufferToGPU();
}

void MeshViewer::createRootSignature()
{
  // use constant buffer as parameter
  CD3DX12_ROOT_PARAMETER param = {};
  param.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

  CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
  descRootSignature.Init(1, &param, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> rootBlob, errorBlob;
  D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob);

  getDevice()->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(),
                                   IID_PPV_ARGS(&m_rootSignature));
}

void MeshViewer::createConstantBufferForEachSwapchainFrame()
{
  // here we need to create n constant buffers with n being the number of swapchain frames
  const auto numSwapchainFrames = getDX12AppConfig().frameCount;
  m_constantBuffers.resize(numSwapchainFrames);

  for (size_t i = 0; i < numSwapchainFrames; i++)
  {
    const auto constantBufferDescription = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ConstantBuffer));
    const auto uploadHeapProperties      = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    getDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &constantBufferDescription,
                                         D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                         IID_PPV_ARGS(&m_constantBuffers[i]));
  }
}

void MeshViewer::updateConstantBuffer()
{
  ConstantBuffer cb;
  // std::cout << "Wireframe color is" << to_string(m_uiData.m_wireFrameColor) << std::endl;
  //  cb.wireFrameColor = f32v4(m_uiData.m_wireFrameColor, 1.0f);
  updateUIData(&cb);

  const auto pos_pointer = &m_VertexBufferCPU.data()->position;
  f32m4      modelMatrix = getNormalizationTransformation(pos_pointer, static_cast<ui32>(m_VertexBufferCPU.size()));

  f32m4 viewMatrix = m_examinerController.getTransformationMatrix();

  f32m4 projMatrix =
      glm::perspectiveFovLH_ZO(glm::radians(30.0f), m_uiData.m_viewPortWidth, m_uiData.m_viewPortHeight, 0.0f, 100.0f);
  const auto mv  = viewMatrix * modelMatrix;
  const auto mvp = projMatrix * mv;

  // MVP matrix
  cb.mv  = mv;
  cb.mvp = mvp;

  const auto& currentConstantBuffer = m_constantBuffers[this->getFrameIndex()];
  void*       p;
  currentConstantBuffer->Map(0, nullptr, &p);
  ::memcpy(p, &cb, sizeof(cb));
  currentConstantBuffer->Unmap(0, nullptr);
}

void MeshViewer::updateUIData(ConstantBuffer* cb)
{
  cb->wireFrameColor = f32v4(m_uiData.m_wireFrameColor, 1.0f);
  // cb->diffuseColor   = f32v4(m_uiData.m_diffuseColor, 1.0f);
  // cb->ambientColor   = f32v4(m_uiData.m_ambientColor, 1.0f);
}

void MeshViewer::createPipeline()
{
  const auto vertexShader =
      compileShader(L"../../../Assignments/A0MeshViewer/Shaders/TriangleMesh.hlsl", L"VS_main", L"vs_6_0");

  const auto pixelShader =
      compileShader(L"../../../Assignments/A0MeshViewer/Shaders/TriangleMesh.hlsl", L"PS_main", L"ps_6_0");

  /*D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};*/

  D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout                        = {inputElementDescs, _countof(inputElementDescs)};
  psoDesc.pRootSignature                     = m_rootSignature.Get();
  psoDesc.VS                                 = HLSLCompiler::convert(vertexShader);
  psoDesc.PS                                 = HLSLCompiler::convert(pixelShader);
  psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.CullMode           = D3D12_CULL_MODE_NONE;
  psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState                  = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.SampleMask                         = UINT_MAX;
  psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets                   = 1;
  psoDesc.SampleDesc.Count                   = 1;
  psoDesc.RTVFormats[0]                      = getDX12AppConfig().renderTargetFormat;
  psoDesc.DSVFormat                          = getDX12AppConfig().depthBufferFormat;
  psoDesc.DepthStencilState.DepthEnable      = TRUE;
  psoDesc.DepthStencilState.DepthFunc        = D3D12_COMPARISON_FUNC_ALWAYS;
  psoDesc.DepthStencilState.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ALL;
  psoDesc.DepthStencilState.StencilEnable    = FALSE;

  throwIfFailed(getDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

void MeshViewer::createWireFramePipeline()
{
  const auto vertexShader =
      compileShader(L"../../../Assignments/A0MeshViewer/Shaders/TriangleMesh.hlsl", L"VS_WireFrame_main", L"vs_6_0");

  const auto pixelShader =
      compileShader(L"../../../Assignments/A0MeshViewer/Shaders/TriangleMesh.hlsl", L"PS_WireFrame_main", L"ps_6_0");

  D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
  ;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout                        = {inputElementDescs, _countof(inputElementDescs)};
  psoDesc.pRootSignature                     = m_rootSignature.Get();
  psoDesc.VS                                 = HLSLCompiler::convert(vertexShader);
  psoDesc.PS                                 = HLSLCompiler::convert(pixelShader);
  psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.CullMode           = D3D12_CULL_MODE_NONE;
  psoDesc.RasterizerState.FillMode           = D3D12_FILL_MODE_WIREFRAME;
  psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState                  = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.SampleMask                         = UINT_MAX;
  psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets                   = 1;
  psoDesc.SampleDesc.Count                   = 1;
  psoDesc.RTVFormats[0]                      = getRenderTarget()->GetDesc().Format;
  psoDesc.DSVFormat                          = getDepthStencil()->GetDesc().Format;
  psoDesc.DepthStencilState.DepthEnable      = FALSE;
  psoDesc.DepthStencilState.DepthFunc        = D3D12_COMPARISON_FUNC_ALWAYS;
  psoDesc.DepthStencilState.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ZERO;
  psoDesc.DepthStencilState.StencilEnable    = FALSE;

  throwIfFailed(getDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_wireFramePipelineState)));
}

void MeshViewer::initializeVertexBuffer(const CograBinaryMeshFile* cbm)
{
  const auto numVertices   = cbm->getNumVertices();
  const auto vertexPointer = cbm->getPositionsPtr();
  const auto normalPointer = reinterpret_cast<f32v3*>(cbm->getAttributePtr(0));
  // const auto texture_coordinates = cbm->getAttributePtr(1);

  if (numVertices % 3 != 0)
  {
    std::cerr << "The loaded vertex data is invalid. Please check for errors..." << std::endl;
    exit(1);
  }
  // convert simple float vector to vertex vector
  for (size_t i = 0; i < numVertices; i++)
  {
    Vertex v;
    v.position = {vertexPointer[i * 3], vertexPointer[i * 3 + 1], vertexPointer[i * 3 + 2]};
    v.normal   = normalPointer[i];
    m_VertexBufferCPU.push_back(v);
  }

  m_vertexBufferSize = m_VertexBufferCPU.size() * sizeof(Vertex);
}

void MeshViewer::uploadVertexBufferToGPU()
{
  //// apply transformation
  // //f32m4 T = getNormalizationTransformation(m_VertexBufferCPU.data(), m_VertexBufferCPU.size());
  // f32m4 viewMatrix = m_examinerController.getTransformationMatrix();

  //// Projection matrix - perspective projection for 3D depth
  //// float aspectRatio = static_cast<float>(getViewport().Width) / static_cast<float>(getViewport().Height);
  ////f32   aspectRatio = m_uiData.m_width / m_uiData.m_height;

  ////f32m4 projMatrix  = glm::perspective(glm::radians(90.0f), aspectRatio, 0.1f, 100.0f);
  // f32m4 projMatrix = glm::perspectiveFovLH_ZO(glm::radians(90.0f), m_uiData.m_width, m_uiData.m_height, 0.0f,
  // 100.0f); const auto vp                 = viewMatrix * projMatrix; const auto temp_vertex_buffer =
  // m_VertexBufferCPU; for (ui32 i = 0; i < m_VertexBufferCPU.size(); i++)
  //{
  //   //f32m4 normalizationTransformation =
  //   //    getNormalizationTransformation(&m_VertexBufferCPU.at(i).position, )
  //   m_VertexBufferCPU.at(i).position = vp * f32v4(m_VertexBufferCPU.at(i).position, 1.0f);
  // }

  UploadHelper uploadHelper(getDevice(), m_vertexBufferSize);

  // 1. Create vertex buffer description (resource description)
  const auto vertexBufferDescription = CD3DX12_RESOURCE_DESC::Buffer(m_vertexBufferSize);

  // 2. Create default heap properties
  const auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

  getDevice()->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDescription,
                                       D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_vertexBufferGPU));

  uploadHelper.uploadBuffer(m_VertexBufferCPU.data(), m_vertexBufferGPU, m_vertexBufferSize, getCommandQueue());
  m_vertexBufferView.BufferLocation = m_vertexBufferGPU->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes    = static_cast<ui32>(m_vertexBufferSize);
  m_vertexBufferView.StrideInBytes  = sizeof(Vertex);

  // m_VertexBufferCPU = temp_vertex_buffer;
}

void MeshViewer::initializeIndexBuffer(const CograBinaryMeshFile* cbm)
{
  const auto numIndices         = cbm->getNumTriangles() * 3;
  const auto indexBufferPointer = cbm->getTriangleIndices();
  for (ui32 i = 0; i < numIndices; i++)
  {
    m_indexBuffer.push_back(indexBufferPointer[i]);
  }
  m_indexBufferSize = m_indexBuffer.size() * sizeof(ui32);
}

void MeshViewer::uploadIndexBufferToGPU()
{
  UploadHelper uploadHelper(getDevice(), m_indexBufferSize);

  // 1. Create index buffer description
  const auto indexBufferDescription = CD3DX12_RESOURCE_DESC::Buffer(m_indexBufferSize);

  // 2. Create upload heap properties
  const auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

  getDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &indexBufferDescription,
                                       D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_indexBufferGPU));

  uploadHelper.uploadBuffer(m_indexBuffer.data(), m_indexBufferGPU, m_indexBufferSize, getCommandQueue());
  m_indexBufferView.BufferLocation = m_indexBufferGPU->GetGPUVirtualAddress();
  m_indexBufferView.SizeInBytes    = static_cast<ui32>(m_indexBufferSize);
  m_indexBufferView.Format         = DXGI_FORMAT_R32_UINT;
}

MeshViewer::~MeshViewer()
{
}

void MeshViewer::onDraw()
{
  if (m_uiData.m_viewPortHeight == 0 || m_uiData.m_viewPortWidth == 0)
  {
    return;
  }
  // uploadVertexBufferToGPU();
  if (!ImGui::GetIO().WantCaptureMouse)
  {
    bool pressed  = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    bool released = ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Right);
    if (pressed || released)
    {
      bool left = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Left);
      m_examinerController.click(pressed, left == true ? 1 : 2,
                                 ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl),
                                 getNormalizedMouseCoordinates());
    }
    else
    {
      m_examinerController.move(getNormalizedMouseCoordinates());
    }
  }

  // TODO Implement me!
  // Use this to get the transformation Matrix.
  // Of course, skip the (void). That is just to prevent warning, sinces I am not using it here (but you will have to!)
  //(void)m_examinerController.getTransformationMatrix();

  const auto commandList = getCommandList();
  const auto rtvHandle   = getRTVHandle();
  const auto dsvHandle   = getDSVHandle();
  // TODO Implement me!

  commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
  // TODO Implement me!

  const float clearColor[] = {m_uiData.m_backgroundColor.x, m_uiData.m_backgroundColor.y, m_uiData.m_backgroundColor.z,
                              1.0f};
  commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  commandList->RSSetViewports(1, &getViewport());
  commandList->RSSetScissorRects(1, &getRectScissor());

  commandList->SetPipelineState(m_pipelineState.Get());
  commandList->SetGraphicsRootSignature(m_rootSignature.Get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
  commandList->IASetIndexBuffer(&m_indexBufferView);
  commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffers[getFrameIndex()]->GetGPUVirtualAddress());
  updateConstantBuffer();

  commandList->DrawIndexedInstanced(static_cast<ui32>(m_indexBuffer.size()), 1, 0, 0, 0);

  if (m_uiData.m_wireFrameEnabled)
  {
    commandList->SetPipelineState(m_wireFramePipelineState.Get());
    commandList->DrawIndexedInstanced(static_cast<ui32>(m_indexBuffer.size()), 1, 0, 0, 0);
  }
}

void MeshViewer::onDrawUI()
{
  const auto imGuiFlags = m_examinerController.active() ? ImGuiWindowFlags_NoInputs : ImGuiWindowFlags_None;

  m_uiData.m_viewPortHeight = ImGui::GetMainViewport()->WorkSize.y;
  m_uiData.m_viewPortWidth  = ImGui::GetMainViewport()->WorkSize.x;

  // Information window
  ImGui::Begin("Information", nullptr, imGuiFlags);
  ImGui::Text("Frametime: %f", 1.0f / ImGui::GetIO().Framerate * 1000.0f);
  ImGui::End();

  // Configuration window
  ImGui::Begin("Configuration", nullptr, imGuiFlags);
  ImGui::ColorEdit3("Background Color", &m_uiData.m_backgroundColor.x);
  ImGui::Checkbox("Back-Face Culling", &m_uiData.m_backFaceCullingEnabled);
  ImGui::Checkbox("Overlay Wireframe", &m_uiData.m_wireFrameEnabled);
  ImGui::ColorEdit3("Wireframe Color", &m_uiData.m_wireFrameColor.x);
  ImGui::Checkbox("Two-Sided Lighting", &m_uiData.m_twoSidedLightingEnabled);
  ImGui::Checkbox("Use Texture", &m_uiData.m_useTextureEnabled);
  ImGui::ColorEdit3("Ambient", &m_uiData.m_ambientColor.x);
  ImGui::ColorEdit3("Diffuse", &m_uiData.m_diffuseColor.x);
  ImGui::ColorEdit3("Specular", &m_uiData.m_specularColor.x);
  ImGui::End();
}