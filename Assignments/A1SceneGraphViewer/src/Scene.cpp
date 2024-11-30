#include "Scene.hpp"
#include <d3dx12/d3dx12.h>
#include <unordered_map>

using namespace gims;

namespace
{
void addToCommandListImpl(Scene& scene, ui32 nodeIdx, f32m4 accuTransformation,
                          const ComPtr<ID3D12GraphicsCommandList>& commandList, ui32 modelViewRootParameterIdx,
                          ui32 materialConstantsRootParameterIdx, ui32 srvRootParameterIdx)
{
  (void)scene;
  (void)nodeIdx;
  (void)accuTransformation;
  (void)commandList;
  (void)modelViewRootParameterIdx;
  (void)materialConstantsRootParameterIdx;
  (void)srvRootParameterIdx;
  // Assignment 6

  if (nodeIdx >= scene.getNumberOfNodes())
  {
    return;
  }

  const auto& currentNode = scene.getNode(nodeIdx);
  // update transformation
  accuTransformation = accuTransformation * currentNode.transformation;

  // draw meshes
  for (ui32 m = 0; m < (ui32)currentNode.meshIndices.size(); m++)
  {
    commandList->SetGraphicsRoot32BitConstants(modelViewRootParameterIdx, 16, &accuTransformation, 0);
    // draw call
    scene.getMesh(currentNode.meshIndices[m]).addToCommandList(commandList);
  }

  for (ui32 c = 0; c < (ui32)currentNode.childIndices.size(); c++)
  {
    addToCommandListImpl(scene, currentNode.childIndices[c], accuTransformation, commandList, modelViewRootParameterIdx,
                         materialConstantsRootParameterIdx, srvRootParameterIdx);
  }

  // TODO: Assignment 10
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

void Scene::addToCommandList(const ComPtr<ID3D12GraphicsCommandList>& commandList, const f32m4 transformation,
                             ui32 modelViewRootParameterIdx, ui32 materialConstantsRootParameterIdx,
                             ui32 srvRootParameterIdx)
{
  addToCommandListImpl(*this, 0, transformation, commandList, modelViewRootParameterIdx,
                       materialConstantsRootParameterIdx, srvRootParameterIdx);
}
} // namespace gims
