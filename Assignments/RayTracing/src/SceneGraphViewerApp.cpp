#include "SceneGraphViewerApp.hpp"
#include "RayTracingUtils.hpp"
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
    , m_rayTracingUtils(RayTracingUtils::createRayTracingUtils(getDevice(), m_scene, getCommandList(),
                                                               getCommandAllocator(), getCommandQueue(), getHeight(),
                                                               getWidth(), (*this)))
{
  m_examinerController.setTranslationVector(f32v3(0, -0.25f, 1.5));
  m_uiData.m_useNormalMapping = false;
  createRootSignatures();
  createSceneConstantBuffer();
  createPipeline();
}

#pragma region Init

void SceneGraphViewerApp::createRootSignatures()
{
  // graphics root signature
  CD3DX12_ROOT_PARAMETER   rootParameter[5] = {};
  CD3DX12_DESCRIPTOR_RANGE range            = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0};
  rootParameter[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);   // scene constant buffer
  rootParameter[1].InitAsConstants(32, 1, D3D12_ROOT_SIGNATURE_FLAG_NONE);        // mv matrix
  rootParameter[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_PIXEL); // materials
  rootParameter[3].InitAsDescriptorTable(1, &range);                              // textures
  rootParameter[4].InitAsShaderResourceView(5);                                   // TLAS

  D3D12_STATIC_SAMPLER_DESC sampler = {};
  sampler.Filter                    = D3D12_FILTER_MIN_MAG_MIP_POINT;
  sampler.AddressU                  = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler.AddressV                  = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler.AddressW                  = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler.MipLODBias                = 0;
  sampler.MaxAnisotropy             = 0;
  sampler.ComparisonFunc            = D3D12_COMPARISON_FUNC_NEVER;
  sampler.BorderColor               = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
  sampler.MinLOD                    = 0.0f;
  sampler.MaxLOD                    = D3D12_FLOAT32_MAX;
  sampler.ShaderRegister            = 0;
  sampler.RegisterSpace             = 0;
  sampler.ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

  CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
  rootSignatureDesc.Init(_countof(rootParameter), rootParameter, 1, &sampler,
                         D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> rootBlob, errorBlob;
  D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob);

  getDevice()->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(),
                                   IID_PPV_ARGS(&m_graphicsRootSignature));

  //// compute root signature
  // CD3DX12_ROOT_PARAMETER rayTracingRootParameter[1] = {};
  // rayTracingRootParameter[0].InitAsShaderResourceView(0); // TLAS bound to t3, space0

  // CD3DX12_ROOT_SIGNATURE_DESC rayTracingRootSignatureDesc = {};
  // rayTracingRootSignatureDesc.Init(_countof(rayTracingRootParameter), rayTracingRootParameter, 0, nullptr,
  //                        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  // D3D12SerializeRootSignature(&rayTracingRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob);

  // getDevice()->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(),
  //                                  IID_PPV_ARGS(&m_computeRootSignature));
}

void SceneGraphViewerApp::createPipeline()
{
  waitForGPU();
  const auto inputElementDescs = TriangleMeshD3D12::getInputElementDescriptors();

  const auto vertexShader =
      compileShader(L"../../../Assignments/RayTracing/Shaders/TriangleMesh.hlsl", L"VS_main", L"vs_6_8");
  const auto pixelShader =
      compileShader(L"../../../Assignments/RayTracing/Shaders/TriangleMesh.hlsl", L"PS_main", L"ps_6_8");

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout                        = {inputElementDescs.data(), (ui32)inputElementDescs.size()};
  psoDesc.pRootSignature                     = m_graphicsRootSignature.Get();
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
  ImGui::Checkbox("Use normal mapping (not really working)", &m_uiData.m_useNormalMapping);
  ImGui::SliderFloat("Light angle phi", &m_uiData.m_lightAngles.x, 0.0f, 180.0f);
  ImGui::SliderFloat("Light angle theta", &m_uiData.m_lightAngles.y, 0.0f, 360.0f);
  
  ImGui::End();
}

void SceneGraphViewerApp::drawScene(const ComPtr<ID3D12GraphicsCommandList>& cmdLst)
{
  const auto cameraMatrix = m_examinerController.getTransformationMatrix();
  updateSceneConstantBuffer();

  //  Assignment 6 (normalize scene)
  const auto modelMatrix            = m_scene.getAABB().getNormalizationTransformation();
  const auto cameraAndNormalization = cameraMatrix * modelMatrix;

  cmdLst->SetPipelineState(m_pipelineState.Get());

  cmdLst->SetGraphicsRootSignature(m_graphicsRootSignature.Get());

  // set constant buffer
  const auto sceneCb = constantBuffers[getFrameIndex()].getResource()->GetGPUVirtualAddress();
  cmdLst->SetGraphicsRootConstantBufferView(0, sceneCb);

  // ray tracing
  cmdLst->SetGraphicsRootShaderResourceView(4, m_rayTracingUtils.m_topLevelAS->GetGPUVirtualAddress());

  m_scene.addToCommandList(cmdLst, cameraAndNormalization, modelMatrix, 1, 2, 3);
}

#pragma endregion

#pragma region Constant Buffer

namespace
{
struct SceneConstantBuffer
{
  f32m4 projectionMatrix;
  f32v3 lightDirection;
  ui8   flags;
};

} // namespace

void SceneGraphViewerApp::createSceneConstantBuffer()
{
  const SceneConstantBuffer cb         = {};
  const auto                frameCount = getDX12AppConfig().frameCount;
  constantBuffers.resize(frameCount);
  for (ui32 i = 0; i < frameCount; i++)
  {
    constantBuffers[i] = ConstantBufferD3D12(cb, getDevice());
  }
}

void SceneGraphViewerApp::updateSceneConstantBuffer()
{
  SceneConstantBuffer cb;

  const auto cx = cos(glm::radians(m_uiData.m_lightAngles.x));
  const auto sx = sin(glm::radians(m_uiData.m_lightAngles.x));
  const auto cy = cos(glm::radians(m_uiData.m_lightAngles.y));
  const auto sy = sin(glm::radians(m_uiData.m_lightAngles.y));

  cb.lightDirection = f32v3(sx * cy, sx * sy, cx);
  cb.flags          = m_uiData.m_useNormalMapping & 0x1;
  cb.projectionMatrix =
      glm::perspectiveFovLH_ZO<f32>(glm::radians(45.0f), (f32)getWidth(), (f32)getHeight(), 0.01f, 1000.0f);
  // std::cout << "Projection is " << glm::to_string(cb.projectionMatrix) << std::endl;
  constantBuffers[getFrameIndex()].upload(&cb);
}

#pragma endregion
