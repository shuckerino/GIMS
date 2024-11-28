#include "SceneGraphViewerApp.hpp"
#include "SceneFactory.hpp"
#include <d3dx12/d3dx12.h>
#include <gimslib/contrib/stb/stb_image.h>
#include <gimslib/d3d/DX12Util.hpp>
#include <gimslib/d3d/UploadHelper.hpp>
#include <gimslib/dbg/HrException.hpp>
#include <gimslib/io/CograBinaryMeshFile.hpp>
#include <gimslib/sys/Event.hpp>
#include <imgui.h>
#include <iostream>
#include <vector>
using namespace gims;

SceneGraphViewerApp::SceneGraphViewerApp(const DX12AppConfig config, const std::filesystem::path pathToScene)
    : DX12App(config)
    , m_examinerController(true)
    , m_scene(SceneGraphFactory::createFromAssImpScene(pathToScene, getDevice(), getCommandQueue()))
{
  m_examinerController.setTranslationVector(f32v3(0, -0.25f, 1.5));
  createRootSignature();
  createSceneConstantBuffer();
  createPerMeshConstantBuffer();
  createPipeline();
}

#pragma region Init

void SceneGraphViewerApp::createRootSignature()
{
  //// Descriptor Range für Constant Buffers b1, b2
  // D3D12_DESCRIPTOR_RANGE descriptorRangeCBV            = {};
  // descriptorRangeCBV.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
  // descriptorRangeCBV.NumDescriptors                    = 2; // Zwei Constant Buffers
  // descriptorRangeCBV.BaseShaderRegister                = 1; // b1
  // descriptorRangeCBV.RegisterSpace                     = 0;
  // descriptorRangeCBV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  //// Descriptor Range für Texturen (t0 bis t4)
  // D3D12_DESCRIPTOR_RANGE descriptorRangeSRV            = {};
  // descriptorRangeSRV.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  // descriptorRangeSRV.NumDescriptors                    = 5; // Fünf Texturen
  // descriptorRangeSRV.BaseShaderRegister                = 0; // t0
  // descriptorRangeSRV.RegisterSpace                     = 0;
  // descriptorRangeSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  //// Root Parameter für Constant Buffers
  // D3D12_ROOT_PARAMETER rootParameters[3] = {};

  //// First parameter needs to be Scene constant buffer because of draw call
  // rootParameters[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
  // rootParameters[0].Descriptor.ShaderRegister = 0; // b0
  // rootParameters[0].Descriptor.RegisterSpace  = 0;
  // rootParameters[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

  //// Other two constant Buffers (Descriptor Table)
  // rootParameters[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  // rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
  // rootParameters[1].DescriptorTable.pDescriptorRanges   = &descriptorRangeCBV;
  // rootParameters[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;

  //// Texturen (Descriptor Table)
  // rootParameters[2].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  // rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
  // rootParameters[2].DescriptorTable.pDescriptorRanges   = &descriptorRangeSRV;
  // rootParameters[2].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

  //// Sampler (Descriptor Table)
  // D3D12_STATIC_SAMPLER_DESC sampler = {};
  // sampler.Filter                    = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  // sampler.AddressU                  = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  // sampler.AddressV                  = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  // sampler.AddressW                  = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  // sampler.MipLODBias                = 0;
  // sampler.MaxAnisotropy             = 1;
  // sampler.ComparisonFunc            = D3D12_COMPARISON_FUNC_ALWAYS;
  // sampler.BorderColor               = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
  // sampler.MinLOD                    = 0.0f;
  // sampler.MaxLOD                    = D3D12_FLOAT32_MAX;
  // sampler.ShaderRegister            = 0; // s0
  // sampler.RegisterSpace             = 0;
  // sampler.ShaderVisibility          = D3D12_SHADER_VISIBILITY_PIXEL;

  //// Root Signature Description
  // D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
  // rootSignatureDesc.NumParameters             = _countof(rootParameters);
  // rootSignatureDesc.pParameters               = rootParameters;
  // rootSignatureDesc.NumStaticSamplers         = 1;
  // rootSignatureDesc.pStaticSamplers           = &sampler;
  // rootSignatureDesc.Flags                     = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // Root Signature Description
  // D3D12_ROOT_PARAMETER rootParameter = {};

  CD3DX12_ROOT_PARAMETER rootParameter[2] = {};
  rootParameter[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
  rootParameter[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);

  CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
  rootSignatureDesc.Init(2, rootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> rootBlob, errorBlob;
  D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob);

  getDevice()->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(),
                                   IID_PPV_ARGS(&m_rootSignature));
}

void SceneGraphViewerApp::createPipeline()
{
  waitForGPU();
  const auto inputElementDescs = TriangleMeshD3D12::getInputElementDescriptors();

  const auto vertexShader =
      compileShader(L"../../../Assignments/A1SceneGraphViewer/Shaders/TriangleMesh.hlsl", L"VS_main", L"vs_6_0");
  const auto pixelShader =
      compileShader(L"../../../Assignments/A1SceneGraphViewer/Shaders/TriangleMesh.hlsl", L"PS_main", L"ps_6_0");

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout                        = {inputElementDescs.data(), (ui32)inputElementDescs.size()};
  psoDesc.pRootSignature                     = m_rootSignature.Get();
  psoDesc.VS                                 = HLSLCompiler::convert(vertexShader);
  psoDesc.PS                                 = HLSLCompiler::convert(pixelShader);
  psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.FillMode           = D3D12_FILL_MODE_SOLID;
  psoDesc.RasterizerState.CullMode           = D3D12_CULL_MODE_NONE;
  psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DSVFormat                          = getDX12AppConfig().depthBufferFormat;
  psoDesc.DepthStencilState.DepthEnable      = TRUE;
  psoDesc.DepthStencilState.DepthFunc        = D3D12_COMPARISON_FUNC_LESS;
  psoDesc.DepthStencilState.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ALL;
  psoDesc.DepthStencilState.StencilEnable    = FALSE;
  psoDesc.SampleMask                         = UINT_MAX;
  psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets                   = 1;
  psoDesc.RTVFormats[0]                      = getDX12AppConfig().renderTargetFormat;
  psoDesc.SampleDesc.Count                   = 1;
  throwIfFailed(getDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

#pragma endregion

#pragma region OnDraw

void SceneGraphViewerApp::onDraw()
{
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

  const auto commandList = getCommandList();
  const auto rtvHandle   = getRTVHandle();
  const auto dsvHandle   = getDSVHandle();

  commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

  const float clearColor[] = {m_uiData.m_backgroundColor.x, m_uiData.m_backgroundColor.y, m_uiData.m_backgroundColor.z,
                              1.0f};
  commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  commandList->RSSetViewports(1, &getViewport());
  commandList->RSSetScissorRects(1, &getRectScissor());

  drawScene(commandList);
}

void SceneGraphViewerApp::onDrawUI()
{
  const auto imGuiFlags = m_examinerController.active() ? ImGuiWindowFlags_NoInputs : ImGuiWindowFlags_None;
  ImGui::Begin("Information", nullptr, imGuiFlags);
  ImGui::Text("Frametime: %f", 1.0f / ImGui::GetIO().Framerate * 1000.0f);
  ImGui::End();
  ImGui::Begin("Configuration", nullptr, imGuiFlags);
  ImGui::ColorEdit3("Background Color", &m_uiData.m_backgroundColor[0]);
  ImGui::End();
}

void SceneGraphViewerApp::drawScene(const ComPtr<ID3D12GraphicsCommandList>& cmdLst)
{
  // test assignment 2
  const auto meshToDraw  = m_scene.getMesh(1);
  const auto modelMatrix = meshToDraw.getAABB().getNormalizationTransformation();

  // Assignment 2: Uncomment after successful implementation.
  const auto cameraMatrix = m_examinerController.getTransformationMatrix();
  const auto mv           = cameraMatrix * modelMatrix;
  //std::cout << "Mv is " << glm::to_string(mv) << std::endl;
  updatePerMeshConstantBuffer(mv);
  updateSceneConstantBuffer();
  //  Assignment 6

  cmdLst->SetPipelineState(m_pipelineState.Get());

  // Assignment 2: Uncomment after successful implementation.
  cmdLst->SetGraphicsRootSignature(m_rootSignature.Get());

  // set both constant buffers
  const auto sceneCb = m_sceneconstantBuffers[getFrameIndex()].getResource()->GetGPUVirtualAddress();
  const auto meshCb  = m_meshconstantBuffers[getFrameIndex()].getResource()->GetGPUVirtualAddress();
  cmdLst->SetGraphicsRootConstantBufferView(0, sceneCb);
  cmdLst->SetGraphicsRootConstantBufferView(1, meshCb);

  m_scene.addToCommandList(cmdLst, cameraMatrix, 1, 2, 3);

  // try drawing single meshes for Assignment 2

  meshToDraw.addToCommandList(cmdLst);
}

#pragma endregion

#pragma region Constant Buffer

namespace
{
struct SceneConstantBuffer
{
  f32m4 projectionMatrix;
};

struct PerMeshConstantBuffer
{
  f32m4 modelViewMatrix;
};

} // namespace

void SceneGraphViewerApp::createSceneConstantBuffer()
{
  const SceneConstantBuffer cb         = {};
  const auto                frameCount = getDX12AppConfig().frameCount;
  m_sceneconstantBuffers.resize(frameCount);
  for (ui32 i = 0; i < frameCount; i++)
  {
    m_sceneconstantBuffers[i] = ConstantBufferD3D12(cb, getDevice());
  }
}

void SceneGraphViewerApp::updateSceneConstantBuffer()
{
  SceneConstantBuffer cb;
  cb.projectionMatrix =
      glm::perspectiveFovLH_ZO<f32>(glm::radians(45.0f), (f32)getWidth(), (f32)getHeight(), 0.01f, 1000.0f);
  // std::cout << "Projection is " << glm::to_string(cb.projectionMatrix) << std::endl;
  m_sceneconstantBuffers[getFrameIndex()].upload(&cb);
}

void SceneGraphViewerApp::createPerMeshConstantBuffer()
{
  const PerMeshConstantBuffer cb         = {};
  const auto                  frameCount = getDX12AppConfig().frameCount;
  m_meshconstantBuffers.resize(frameCount);
  for (ui32 i = 0; i < frameCount; i++)
  {
    m_meshconstantBuffers[i] = ConstantBufferD3D12(cb, getDevice());
  }
}

void SceneGraphViewerApp::updatePerMeshConstantBuffer(const f32m4& modelView)
{
  PerMeshConstantBuffer cb;
  cb.modelViewMatrix = modelView;
  m_meshconstantBuffers[getFrameIndex()].upload(&cb);
}

#pragma endregion
