#include "TriangleMeshD3D12.hpp"
#include <d3dx12/d3dx12.h>
#include <gimslib/d3d/UploadHelper.hpp>
namespace
{
struct Vertex
{
  gims::f32v3 position;
  gims::f32v3 normal;
  gims::f32v2 textureCoordinate;
};
} // namespace

namespace gims
{
const std::vector<D3D12_INPUT_ELEMENT_DESC> TriangleMeshD3D12::m_inputElementDescs = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

TriangleMeshD3D12::TriangleMeshD3D12(f32v3 const* const positions, f32v3 const* const normals,
                                     f32v3 const* const textureCoordinates, ui32 nVertices,
                                     ui32v3 const* const indexBuffer, ui32 nIndices, ui32 materialIndex,
                                     const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12CommandQueue>& commandQueue)
    : m_nIndices(nIndices)
    , m_vertexBufferSize(static_cast<ui32>(nVertices * sizeof(Vertex)))
    , m_indexBufferSize(static_cast<ui32>(nIndices * sizeof(ui32)))
    , m_aabb(positions, nVertices)
    , m_materialIndex(materialIndex)
{
#pragma region Vertex Buffer

  // Init vertex buffer
  auto vertexBuffer = std::vector<Vertex>(nVertices);
  for (ui32 i = 0; i < nVertices; i++)
  {
    Vertex v {
        v.position          = positions[i],
        v.normal            = normals[i],
        v.textureCoordinate = textureCoordinates[i],
    };
    vertexBuffer[i] = v;
  }

  m_vertexBufferSize = nVertices * sizeof(Vertex);

  UploadHelper uploadHelperVertexBuffer(device, m_vertexBufferSize);
  // Create resource on GPU
  const CD3DX12_RESOURCE_DESC   vertexBufferDescription = CD3DX12_RESOURCE_DESC::Buffer(m_vertexBufferSize);
  const CD3DX12_HEAP_PROPERTIES defaultHeapProperties   = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDescription,
                                  D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_vertexBuffer));

  // Upload to GPU
  uploadHelperVertexBuffer.uploadBuffer(vertexBuffer.data(), m_vertexBuffer, m_vertexBufferSize, commandQueue);
  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes    = m_vertexBufferSize;
  m_vertexBufferView.StrideInBytes  = sizeof(Vertex);

#pragma endregion

#pragma region Index Buffer

  m_indexBufferSize = nIndices * sizeof(ui32v3);
  UploadHelper                uploadHelperIndexBuffer(device, m_indexBufferSize);
  const CD3DX12_RESOURCE_DESC indexBufferDescription = CD3DX12_RESOURCE_DESC::Buffer(m_indexBufferSize);
  device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &indexBufferDescription,
                                  D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_indexBuffer));

  uploadHelperIndexBuffer.uploadBuffer(indexBuffer, m_indexBuffer, m_indexBufferSize, commandQueue);
  m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
  m_indexBufferView.SizeInBytes    = m_indexBufferSize;
  m_indexBufferView.Format         = DXGI_FORMAT_R32_UINT;

#pragma endregion

}

void TriangleMeshD3D12::addToCommandList(const ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
  commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
  commandList->IASetIndexBuffer(&m_indexBufferView);
}

const AABB TriangleMeshD3D12::getAABB() const
{
  return m_aabb;
}

const ui32 TriangleMeshD3D12::getMaterialIndex() const
{
  return m_materialIndex;
}

const std::vector<D3D12_INPUT_ELEMENT_DESC>& TriangleMeshD3D12::getInputElementDescriptors()
{
  return m_inputElementDescs;
}

TriangleMeshD3D12::TriangleMeshD3D12()
    : m_nIndices(0)
    , m_vertexBufferSize(0)
    , m_indexBufferSize(0)
    , m_materialIndex((ui32)-1)
{
}

} // namespace gims
