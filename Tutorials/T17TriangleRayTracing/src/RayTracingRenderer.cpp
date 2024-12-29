#include "RayTracingRenderer.hpp"
#include <imgui.h>

using namespace gims;
#if 1
namespace
{
// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
  pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
  WCHAR fullName[50];
  if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
  {
    pObject->SetName(fullName);
  }
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
#endif

// Naming helper for ComPtr<T>.
// Assigns the name of the variable as the name of the object.
// The indexed variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x)            SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

} // namespace

#pragma region Helper functions

// Check if can be replaced with UploadHelper
inline void AllocateUploadBuffer(ID3D12Device* pDevice, void* pData, UINT64 datasize, ID3D12Resource** ppResource,
                                 const wchar_t* resourceName = nullptr)
{
  auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto bufferDesc           = CD3DX12_RESOURCE_DESC::Buffer(datasize);
  throwIfFailed(pDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(ppResource)));
  if (resourceName)
  {
    (*ppResource)->SetName(resourceName);
  }
  void* pMappedData;
  (*ppResource)->Map(0, nullptr, &pMappedData);
  memcpy(pMappedData, pData, datasize);
  (*ppResource)->Unmap(0, nullptr);
}

void RayTracingRenderer::AllocateUAVBuffer(ui64 bufferSize, ID3D12Resource** ppResource,
                                           D3D12_RESOURCE_STATES initialResourceState, const wchar_t* resourceName)
{
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Alignment           = 0;
  desc.Width               = bufferSize;
  desc.Height              = 1;
  desc.DepthOrArraySize    = 1;
  desc.MipLevels           = 1;
  desc.Format              = DXGI_FORMAT_UNKNOWN;
  desc.SampleDesc.Count    = 1;
  desc.SampleDesc.Quality  = 0;
  desc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

  throwIfFailed(getDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &desc,
                                                     initialResourceState, nullptr, IID_PPV_ARGS(ppResource)));
  (*ppResource)->SetName(resourceName);
}

#pragma endregion

#pragma region Init

#pragma region Rasterizing

void RayTracingRenderer::createRootSignature()
{
  CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
  CD3DX12_ROOT_PARAMETER      rootParameter[1] = {};
  rootParameter[0].InitAsShaderResourceView(0); // TLAS

  descRootSignature.Init(_countof(rootParameter), rootParameter, 0, nullptr,
                         D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> rootBlob, errorBlob;
  D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob);

  getDevice()->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(),
                                   IID_PPV_ARGS(&m_rootSignature));
}

void RayTracingRenderer::createPipeline()
{
  const auto vertexShader =
      compileShader(L"../../../Tutorials/T17TriangleRayTracing/Shaders/RayTracing.hlsl", L"VS_main", L"vs_6_3");

  const auto pixelShader =
      compileShader(L"../../../Tutorials/T17TriangleRayTracing/Shaders/RayTracing.hlsl", L"PS_main", L"ps_6_8");

  D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      // Per-instance data
      {"INSTANCE_DATA", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
      {"INSTANCE_DATA", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
      {"INSTANCE_DATA", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
      {"INSTANCE_DATA", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
  };

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout                        = {inputElementDescs, _countof(inputElementDescs)};
  psoDesc.pRootSignature                     = m_globalRootSignature.Get();
  psoDesc.VS                                 = HLSLCompiler::convert(vertexShader);
  psoDesc.PS                                 = HLSLCompiler::convert(pixelShader);
  psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.CullMode           = D3D12_CULL_MODE_NONE;
  psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthEnable      = FALSE;
  psoDesc.DepthStencilState.StencilEnable    = FALSE;
  psoDesc.SampleMask                         = UINT_MAX;
  psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets                   = 1;
  psoDesc.SampleDesc.Count                   = 1;
  psoDesc.RTVFormats[0]                      = getRenderTarget()->GetDesc().Format;
  psoDesc.DSVFormat                          = getDepthStencil()->GetDesc().Format;
  throwIfFailed(getDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

#pragma endregion

RayTracingRenderer::RayTracingRenderer(const DX12AppConfig createInfo)
    : DX12App(createInfo)
    , m_examinerController(true)
{
  if (isRayTracingSupported() == false)
  {
    throw std::runtime_error("Ray tracing not supported on this device");
  }

  m_examinerController.setTranslationVector(f32v3(0, 0, 3));

  createRayTracingResources();

  // createRootSignature();
  createPipeline();

  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
  {
    debugController->EnableDebugLayer();
  }
}

bool RayTracingRenderer::isRayTracingSupported()
{
  D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
  throwIfFailed(getDevice()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
  if (options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
  {
    std::cout << "Ray tracing not supported on this device" << std::endl;
    return false;
  }
  else
  {
    std::cout << "Ray tracing supported on your device" << std::endl;
    return true;
  }
}

/// <summary>
/// Initializing all resources needed for ray tracing
/// </summary>
void RayTracingRenderer::createRayTracingResources()
{
  createRootSignatures();
  createGeometry();
  createAccelerationStructures();
}

/// <summary>
/// Method for creating global and local root signatures
/// </summary>
void RayTracingRenderer::createRootSignatures()
{
  CD3DX12_ROOT_PARAMETER rootParameters[2];
  rootParameters[0].InitAsShaderResourceView(0);                            // acceleration structure
  rootParameters[1].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_ALL); // root constants
  CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc;
  globalRootSignatureDesc.Init(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr,
                               D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> rootBlob, errorBlob;
  D3D12SerializeRootSignature(&globalRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob);

  getDevice()->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(),
                                   IID_PPV_ARGS(&m_globalRootSignature));

  std::cout << "Created global root signature" << std::endl;
}

D3D12_RAYTRACING_GEOMETRY_DESC RayTracingRenderer::createGeometryDescription()
{
  D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
  geometryDesc.Type                           = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
  geometryDesc.Triangles.IndexBuffer          = m_indexBuffer->GetGPUVirtualAddress();
  geometryDesc.Triangles.IndexCount           = static_cast<UINT>(m_indexBuffer->GetDesc().Width) / sizeof(ui32);
  geometryDesc.Triangles.IndexFormat          = DXGI_FORMAT_R16_UINT;
  geometryDesc.Triangles.Transform3x4         = 0;
  geometryDesc.Triangles.VertexFormat         = DXGI_FORMAT_R32G32B32_FLOAT;
  geometryDesc.Triangles.VertexCount          = static_cast<UINT>(m_vertexBuffer->GetDesc().Width) / sizeof(Vertex);
  geometryDesc.Triangles.VertexBuffer.StartAddress  = m_vertexBuffer->GetGPUVirtualAddress();
  geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

  // Mark the geometry as opaque.
  // PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing
  // optimizations. Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is
  // present or not.
  geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

  return geometryDesc;
}

void RayTracingRenderer::createAccelerationStructures()
{
  // Reset the command list for the acceleration structure construction.
  getCommandList()->Reset(getCommandAllocator().Get(), nullptr);

  D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
  geometryDesc.Type                           = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
  geometryDesc.Triangles.IndexBuffer          = m_indexBuffer->GetGPUVirtualAddress();
  geometryDesc.Triangles.IndexCount           = static_cast<UINT>(m_indexBuffer->GetDesc().Width) / sizeof(Index);
  geometryDesc.Triangles.IndexFormat          = DXGI_FORMAT_R16_UINT;
  geometryDesc.Triangles.Transform3x4         = 0;
  geometryDesc.Triangles.VertexFormat         = DXGI_FORMAT_R32G32B32_FLOAT;
  geometryDesc.Triangles.VertexCount          = static_cast<UINT>(m_vertexBuffer->GetDesc().Width) / sizeof(Vertex);
  geometryDesc.Triangles.VertexBuffer.StartAddress  = m_vertexBuffer->GetGPUVirtualAddress();
  geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
  geometryDesc.Flags                                = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

  // create 3 instances of BLAS with the same geometry description
  ComPtr<ID3D12Resource> blas[3];
  ComPtr<ID3D12Resource> bottomLevelScratchResources[3];
  for (ui8 i = 0; i < _countof(blas); i++)
  {
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS  bottomLevelInputs       = {};
    bottomLevelInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    bottomLevelInputs.pGeometryDescs = &geometryDesc;
    getDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
    throwIfZero(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    AllocateUAVBuffer(bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &bottomLevelScratchResources[i],
                      D3D12_RESOURCE_STATE_COMMON, L"BLASScratchResource");

    AllocateUAVBuffer(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &blas[i],
                      D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"BottomLevelAccelerationStructure");

    // Bottom Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
    {
      bottomLevelBuildDesc.Inputs                           = bottomLevelInputs;
      bottomLevelBuildDesc.ScratchAccelerationStructureData = bottomLevelScratchResources[i]->GetGPUVirtualAddress();
      bottomLevelBuildDesc.DestAccelerationStructureData    = blas[i]->GetGPUVirtualAddress();
    }

    getCommandList()->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
    auto uav = CD3DX12_RESOURCE_BARRIER::UAV(blas[i].Get());
    getCommandList()->ResourceBarrier(1, &uav);
  }

  // create instance descriptors
  D3D12_RAYTRACING_INSTANCE_DESC instanceDescs[3] = {};

  for (ui8 i = 0; i < _countof(instanceDescs); i++)
  {
    instanceDescs[i].InstanceID                          = i;
    instanceDescs[i].InstanceContributionToHitGroupIndex = 0;
    instanceDescs[i].Flags                               = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    instanceDescs[i].AccelerationStructure               = blas[i]->GetGPUVirtualAddress();
    instanceDescs[i].InstanceMask                        = 1;

    // Set different transformations for each instance.
    float offset                     = static_cast<float>(i) * .2f; // Example offset.
    instanceDescs[i].Transform[0][0] = 1.0f;
    instanceDescs[i].Transform[1][1] = 1.0f;
    instanceDescs[i].Transform[2][2] = 1.0f;
    instanceDescs[i].Transform[0][3] = offset; // Translate along x-axis.
  }

  // Allocate upload buffer for instance descriptors.
  ComPtr<ID3D12Resource> instanceDescsBuffer;
  AllocateUploadBuffer(getDevice().Get(), instanceDescs, sizeof(instanceDescs), &instanceDescsBuffer, L"InstanceDescs");

  // create TLAS
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
  topLevelInputs.DescsLayout                                          = D3D12_ELEMENTS_LAYOUT_ARRAY;
  topLevelInputs.Flags    = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  topLevelInputs.NumDescs = 3;
  topLevelInputs.Type     = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
  getDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
  throwIfZero(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

  ComPtr<ID3D12Resource> topLevelScratchResource;
  AllocateUAVBuffer(topLevelPrebuildInfo.ScratchDataSizeInBytes, &topLevelScratchResource, D3D12_RESOURCE_STATE_COMMON,
                    L"TLASScratchResource");

  AllocateUAVBuffer(topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_topLevelAS,
                    D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"TopLevelAccelerationStructure");

  // Top Level Acceleration Structure desc
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
  {
    topLevelInputs.InstanceDescs                       = instanceDescsBuffer->GetGPUVirtualAddress();
    topLevelBuildDesc.Inputs                           = topLevelInputs;
    topLevelBuildDesc.DestAccelerationStructureData    = m_topLevelAS->GetGPUVirtualAddress();
    topLevelBuildDesc.ScratchAccelerationStructureData = topLevelScratchResource->GetGPUVirtualAddress();
  }

  // Build TLAS.
  getCommandList()->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

  getCommandList()->Close();
  ID3D12CommandList* commandLists[] = {getCommandList().Get()};
  getCommandQueue()->ExecuteCommandLists(1, commandLists);

  // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
  waitForGPU();
}

void RayTracingRenderer::createGeometry()
{
  auto device = getDevice().Get();

  float depthValue = 0.0f;
  float offset     = 0.25f;
  //float planeSize  = 2.0f;
  float triangleDistance = 0.7f;

  // Vertices for 3 triangles and a plane
  Vertex vertices[] = {
      // Triangle (stands on the plane)
      {0.0f, offset, depthValue},  // Top vertex
      {-offset, 0.0f, depthValue}, // Bottom-left vertex
      {offset, 0.0f, depthValue},  // Bottom-right vertex

  //    // Plane
  //    {-planeSize, 0.0f, -planeSize}, // Bottom-left corner
  //    {planeSize, 0.0f, -planeSize},  // Bottom-right corner
  //    {-planeSize, 0.0f, planeSize},  // Top-left corner
  //    {planeSize, 0.0f, planeSize},   // Top-right corner
  };


  // Indices for 3 triangles and a plane
  Index indices[] = {
      // First triangle
      0,
      1,
      2,
  };

  InstanceData instanceData[] = {
      {f32m4(1.0f, 0.0f, 0.0f, 0.0f,
              0.0f, 1.0f, 0.0f, 0.0f,
              0.0f, 0.0f, 1.0f, 0.0f,
              0.0f, 0.0f, 0.0f, 1.0f)}, // Instance 1

      {f32m4(1.0f, 0.0f, 0.0f, 0.0f,
              0.0f, 1.0f, 0.0f, 0.0f,
              0.0f, 0.0f, 1.0f, 0.0f,
              -triangleDistance, 0.0f, 0.0f, 1.0f)}, // Instance 2

      {f32m4(1.0f, 0.0f, 0.0f, 0.0f,
              0.0f, 1.0f, 0.0f, 0.0f,
              0.0f, 0.0f, 1.0f, 0.0f,
              triangleDistance, 0.0f, 0.0f, 1.0f)}, // Instance 3
  };

  m_vertexBufferSize = sizeof(vertices);
  m_indexBufferSize  = sizeof(indices);
  m_numIndices       = _countof(indices);

  AllocateUploadBuffer(device, vertices, sizeof(vertices), &m_vertexBuffer);
  AllocateUploadBuffer(device, indices, sizeof(indices), &m_indexBuffer);
  AllocateUploadBuffer(device, instanceData, sizeof(instanceData), &m_instanceBuffer);

  // create views
  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes    = static_cast<ui32>(m_vertexBufferSize);
  m_vertexBufferView.StrideInBytes  = sizeof(Vertex);

  m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
  m_indexBufferView.SizeInBytes    = static_cast<ui32>(m_indexBufferSize);
  m_indexBufferView.Format         = DXGI_FORMAT_R16_UINT;

  m_instanceBufferView.BufferLocation = m_instanceBuffer->GetGPUVirtualAddress();
  m_instanceBufferView.SizeInBytes    = sizeof(instanceData);
  m_instanceBufferView.StrideInBytes  = sizeof(InstanceData);

}

#pragma endregion

#pragma region OnDraw

void RayTracingRenderer::onDraw()
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

  f32v4 clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
  commandList->ClearRenderTargetView(rtvHandle, &clearColor.x, 0, nullptr);
  commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  commandList->RSSetViewports(1, &getViewport());
  commandList->RSSetScissorRects(1, &getRectScissor());

  commandList->SetPipelineState(m_pipelineState.Get());
  commandList->SetGraphicsRootSignature(m_globalRootSignature.Get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
  commandList->IASetVertexBuffers(1, 1, &m_instanceBufferView);
  commandList->IASetIndexBuffer(&m_indexBufferView);

  // bind tlas for inline ray tracing
  commandList->SetGraphicsRootShaderResourceView(0, m_topLevelAS->GetGPUVirtualAddress());

  // bind root constants
  f32m4 viewMatrix       = m_examinerController.getTransformationMatrix();
  f32m4 projectionMatrix = glm::perspectiveFovLH_ZO(glm::radians(30.0f), static_cast<f32>(getWidth()),
                                                    static_cast<f32>(getHeight()), 0.05f, 1000.0f);
  f32m4 mvpMatrix        = projectionMatrix * viewMatrix;
  commandList->SetGraphicsRoot32BitConstants(1, 16, &mvpMatrix, 0);

  commandList->DrawInstanced(3, 3, 0, 0);
}

void RayTracingRenderer::onDrawUI()
{
  // Implement two render pass?
  // ImGui::Begin("Information", nullptr, ImGuiWindowFlags_None);
  // ImGui::Text("Frametime: %f", 1.0f / ImGui::GetIO().Framerate * 1000.0f);
  // ImGui::End();
}

void RayTracingRenderer::onResize()
{
}

#pragma endregion
#endif
int main(int /* argc*/, char /* **argv */)
{
  gims::DX12AppConfig config;
  config.title    = L"Tutorial 17 RayTracing";
  config.useVSync = true;
  try
  {
    RayTracingRenderer renderer(config);
    renderer.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << "\n";
  }
  if (config.debug)
  {
    DX12Util::reportLiveObjects();
  }
  return 0;
}
