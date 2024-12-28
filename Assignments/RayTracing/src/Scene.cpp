#include "Scene.hpp"
#include <d3dx12/d3dx12.h>
#include <unordered_map>

using namespace gims;

namespace
{
void addToCommandListImpl(Scene& scene, ui32 nodeIdx, f32m4 accuModelView,
                          const ComPtr<ID3D12GraphicsCommandList>& commandList, ui32 modelViewRootParameterIdx,
                          ui32 materialConstantsRootParameterIdx, ui32 srvRootParameterIdx)
{
  if (nodeIdx >= scene.getNumberOfNodes())
  {
    return;
  }

  const auto& currentNode = scene.getNode(nodeIdx);
  const auto  worldTransformation = currentNode.worldSpaceTransformation;
  // update transformation
  accuModelView = accuModelView * currentNode.transformation;

  // draw meshes
  for (ui32 m = 0; m < (ui32)currentNode.meshIndices.size(); m++)
  {
    const auto& meshToDraw   = scene.getMesh(currentNode.meshIndices[m]);
    const auto& meshMaterial = scene.getMaterial(meshToDraw.getMaterialIndex());
    commandList->SetGraphicsRoot32BitConstants(modelViewRootParameterIdx, 16, &accuModelView, 0);
    commandList->SetGraphicsRoot32BitConstants(modelViewRootParameterIdx, 16, &worldTransformation, 16);
    commandList->SetGraphicsRootConstantBufferView(
        2, meshMaterial.materialConstantBuffer.getResource()->GetGPUVirtualAddress());
    
    commandList->SetDescriptorHeaps(1, meshMaterial.srvDescriptorHeap.GetAddressOf());
    commandList->SetGraphicsRootDescriptorTable(3, meshMaterial.srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    
    // draw call
    meshToDraw.addToCommandList(commandList);
  }

  for (ui32 c = 0; c < (ui32)currentNode.childIndices.size(); c++)
  {
    addToCommandListImpl(scene, currentNode.childIndices[c], accuModelView, commandList,
                         modelViewRootParameterIdx,
                         materialConstantsRootParameterIdx, srvRootParameterIdx);
  }
}
} // namespace
namespace gims
{
const Scene::Node& Scene::getNode(ui32 nodeIdx) const
{
  return m_nodes[nodeIdx];
}

Scene::Node& Scene::getNode(ui32 nodeIdx)
{
  return m_nodes[nodeIdx];
}

const ui32 Scene::getNumberOfNodes() const
{
  return static_cast<ui32>(m_nodes.size());
}

const ui32 Scene::getNumberOfMeshes() const
{
  return static_cast<ui32>(m_meshes.size());
}

const TriangleMeshD3D12& Scene::getMesh(ui32 meshIdx) const
{
  return m_meshes[meshIdx];
}

const Scene::Material& Scene::getMaterial(ui32 materialIdx) const
{
  return m_materials[materialIdx];
}

const AABB& Scene::getAABB() const
{
  return m_aabb;
}

void Scene::addToCommandList(const ComPtr<ID3D12GraphicsCommandList>& commandList, const f32m4 modelView,
                             ui32 modelViewRootParameterIdx, ui32 materialConstantsRootParameterIdx,
                             ui32 srvRootParameterIdx)
{
  addToCommandListImpl(*this, 0, modelView, commandList, modelViewRootParameterIdx,
                       materialConstantsRootParameterIdx, srvRootParameterIdx);
}
} // namespace gims
