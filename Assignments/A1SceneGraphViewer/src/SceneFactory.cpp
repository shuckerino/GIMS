#include "SceneFactory.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <d3dx12/d3dx12.h>
#include <gimslib/d3d/UploadHelper.hpp>
#include <gimslib/dbg/HrException.hpp>
#include <iostream>
using namespace gims;

namespace
{
/// <summary>
/// Converts the index buffer required for D3D12 rendering from an aiMesh.
/// </summary>
/// <param name="mesh">The ai mesh containing an index buffer.</param>
/// <returns></returns>
std::vector<ui32v3> getTriangleIndicesFromAiMesh(aiMesh const* const mesh)
{
  std::vector<ui32v3> result;
  if (!mesh->HasFaces())
    return result;

  result.reserve(mesh->mNumFaces);
  for (ui32 i = 0; i < mesh->mNumFaces; i++)
  {
    const aiFace& currentFace = mesh->mFaces[i];
    if (currentFace.mNumIndices == 3)
    {
      result.emplace_back(currentFace.mIndices[0], currentFace.mIndices[1], currentFace.mIndices[2]);
    }
    else
    {
      std::cout << "Not 3 indices" << std::endl;
    }
  }

  return result;
}

void addTextureToDescriptorHeap(const ComPtr<ID3D12Device>& device, aiTextureType aiTextureTypeValue,
                                i32 offsetInDescriptors, aiMaterial const* const inputMaterial,
                                const std::vector<Texture2DD3D12>& m_textures, Scene::Material& material,
                                std::unordered_map<std::filesystem::path, ui32> textureFileNameToTextureIndex,
                                ui32                                            defaultTextureIndex)
{
  if (inputMaterial->GetTextureCount(aiTextureTypeValue) == 0)
  {
    m_textures[defaultTextureIndex].addToDescriptorHeap(device, material.srvDescriptorHeap, offsetInDescriptors);
  }
  else
  {
    aiString path;
    inputMaterial->GetTexture((aiTextureType)aiTextureTypeValue, 0, &path);
    m_textures[textureFileNameToTextureIndex[path.C_Str()]].addToDescriptorHeap(device, material.srvDescriptorHeap,
                                                                                offsetInDescriptors);
  }
}

std::unordered_map<std::filesystem::path, ui32> textureFilenameToIndex(aiScene const* const inputScene)
{
  std::unordered_map<std::filesystem::path, ui32> textureFileNameToTextureIndex;

  ui32 textureIdx = 3;
  for (ui32 mIdx = 0; mIdx < inputScene->mNumMaterials; mIdx++)
  {
    for (ui32 textureType = aiTextureType_NONE; textureType < aiTextureType_UNKNOWN; textureType++)
    {
      for (ui32 i = 0; i < inputScene->mMaterials[mIdx]->GetTextureCount((aiTextureType)textureType); i++)
      {
        aiString path;
        inputScene->mMaterials[mIdx]->GetTexture((aiTextureType)textureType, i, &path);

        const auto texturePathCstr = path.C_Str();
        const auto textureIter     = textureFileNameToTextureIndex.find(texturePathCstr);
        if (textureIter == textureFileNameToTextureIndex.end())
        {
          textureFileNameToTextureIndex.emplace(texturePathCstr, static_cast<ui32>(textureIdx));
          textureIdx++;
        }
      }
    }
  }
  return textureFileNameToTextureIndex;
}
/// <summary>
/// Reads the color from the Asset Importer specific (pKey, type, idx) triple.
/// Use the Asset Importer Macros AI_MATKEY_COLOR_AMBIENT, AI_MATKEY_COLOR_DIFFUSE, etc. which map to these arguments
/// correctly.
///
/// If that key does not exist a null vector is returned.
/// </summary>
/// <param name="pKey">Asset importer specific parameter</param>
/// <param name="type"></param>
/// <param name="idx"></param>
/// <param name="material">The material from which we wish to extract the color.</param>
/// <returns>Color or 0 vector if no color exists.</returns>
f32v4 getColor(char const* const pKey, unsigned int type, unsigned int idx, aiMaterial const* const material)
{
  aiColor3D color;
  if (material->Get(pKey, type, idx, color) == aiReturn_SUCCESS)
  {
    return f32v4(color.r, color.g, color.b, 0.0f);
  }
  else
  {
    return f32v4(0.0f);
  }
}

} // namespace

namespace gims
{
Scene SceneGraphFactory::createFromAssImpScene(const std::filesystem::path       pathToScene,
                                               const ComPtr<ID3D12Device>&       device,
                                               const ComPtr<ID3D12CommandQueue>& commandQueue)
{
  Scene outputScene;

  const auto absolutePath = std::filesystem::weakly_canonical(pathToScene);
  if (!std::filesystem::exists(absolutePath))
  {
    throw std::exception((absolutePath.string() + std::string(" does not exist.")).c_str());
  }

  const auto arguments = aiPostProcessSteps::aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                         aiProcess_GenUVCoords | aiProcess_ConvertToLeftHanded | aiProcess_OptimizeMeshes |
                         aiProcess_RemoveRedundantMaterials | aiProcess_ImproveCacheLocality |
                         aiProcess_FindInvalidData | aiProcess_FindDegenerates;

  Assimp::Importer imp;
  imp.SetPropertyBool(AI_CONFIG_PP_FD_REMOVE, true);
  auto inputScene = imp.ReadFile(absolutePath.string(), arguments);
  if (!inputScene)
  {
    throw std::exception((absolutePath.string() + std::string(" can't be loaded. with Assimp.")).c_str());
  }
  const auto textureFileNameToTextureIndex = textureFilenameToIndex(inputScene);

  createMeshes(inputScene, device, commandQueue, outputScene);

  createNodes(inputScene, outputScene, inputScene->mRootNode);

  std::cout << outputScene.m_nodes.size() << std::endl;

  computeSceneAABB(outputScene, outputScene.m_aabb, 0, glm::identity<f32m4>());
  createTextures(textureFileNameToTextureIndex, absolutePath.parent_path(), device, commandQueue, outputScene);
  createMaterials(inputScene, textureFileNameToTextureIndex, device, outputScene);

  return outputScene;
}

/// <summary>
/// Method calling TriangleMeshD3D12::TriangleMeshD3D12 for each mesh in inputScene
/// </summary>
/// <param name="inputScene"></param>
/// <param name="device"></param>
/// <param name="commandQueue"></param>
/// <param name="outputScene"></param>
void SceneGraphFactory::createMeshes(aiScene const* const inputScene, const ComPtr<ID3D12Device>& device,
                                     const ComPtr<ID3D12CommandQueue>& commandQueue, Scene& outputScene)
{
  for (ui32 i = 0; i < inputScene->mNumMeshes; i++)
  {
    const aiMesh* currentMesh = inputScene->mMeshes[i];

    // get positions and normals
    const ui32         numVertices = currentMesh->mNumVertices;
    std::vector<f32v3> positions;
    positions.reserve(numVertices);
    std::vector<f32v3> normals;
    normals.reserve(numVertices);
    std::vector<f32v3> textureCoords;
    textureCoords.reserve(numVertices);
    for (ui32 n = 0; n < numVertices; n++)
    {
      const aiVector3D& currentPos = currentMesh->mVertices[n];
      positions.emplace_back(currentPos.x, currentPos.y, currentPos.z);

      if (currentMesh->HasNormals())
      {
        const aiVector3D& currentNormal = currentMesh->mNormals[n];
        normals.emplace_back(currentNormal.x, currentNormal.y, currentNormal.z);
      }
      else
      {
        normals.emplace_back(0.0f, 0.0f, 0.0f); // Default normal if missing
      }

      if (currentMesh->HasTextureCoords(0))
      {
        const aiVector3D& currentTexCoord = currentMesh->mTextureCoords[0][n];
        textureCoords.emplace_back(currentTexCoord.x, currentTexCoord.y, 0.0f);
      }
      else
      {
        textureCoords.emplace_back(0.0f, 0.0f, 0.0f); // Default UV if missing
      }
    }

    // get triangle indices
    const std::vector<ui32v3> indexBuffer = getTriangleIndicesFromAiMesh(currentMesh);
    const ui32                numIndices  = 3 * static_cast<ui32>(indexBuffer.size()); // mul by 3, because of vec3

    std::cout << "NumVertices: " << numVertices << std::endl;
    std::cout << "NumIndices: " << numIndices << std::endl;

    // create internal mesh
    TriangleMeshD3D12 createdMesh = TriangleMeshD3D12::TriangleMeshD3D12(
        positions.data(), normals.data(), textureCoords.data(), numVertices, indexBuffer.data(), numIndices,
        currentMesh->mMaterialIndex, device, commandQueue);

    outputScene.m_meshes.push_back(createdMesh);
  }
}

ui32 SceneGraphFactory::createNodes(aiScene const* const inputScene, Scene& outputScene, aiNode const* const assimpNode)
{
  (void)inputScene;

  // create node and add to list
  outputScene.m_nodes.emplace_back();
  const auto   currentNodeIndex = static_cast<ui32>(outputScene.m_nodes.size() - 1);
  Scene::Node& currentNode      = outputScene.m_nodes.back();

  // set transformation to parent
  const aiMatrix4x4 parentRelativeTransformation = assimpNode->mTransformation;
  glm::mat4         convertedAssimpTrafo         = aiMatrix4x4ToGlm(parentRelativeTransformation);
  currentNode.transformation                     = convertedAssimpTrafo;

  // set mesh indices
  for (ui32 i = 0; i < assimpNode->mNumMeshes; i++)
  {
    currentNode.meshIndices.push_back(assimpNode->mMeshes[i]);
  }

  // traverse children
  aiNode** childrenOfInputNode = assimpNode->mChildren;
  for (ui32 i = 0; i < assimpNode->mNumChildren; i++)
  {
    const ui32 childNodeIndex = createNodes(inputScene, outputScene, childrenOfInputNode[i]);
    outputScene.m_nodes.at(currentNodeIndex).childIndices.emplace_back(childNodeIndex);
  }

  return currentNodeIndex;
}

void SceneGraphFactory::computeSceneAABB(Scene& scene, AABB& accuAABB, ui32 nodeIdx, f32m4 accuTransformation)
{
  // get current node
  const auto currentNode = scene.getNode(nodeIdx);

  // update transformation
  accuTransformation = accuTransformation * currentNode.transformation;

  // merge aabb for each mesh
  for (const auto& meshIndex : currentNode.meshIndices)
  {
    const auto transformedMeshAABB = scene.m_meshes[meshIndex].getAABB().getTransformed(accuTransformation);
    accuAABB                       = accuAABB.getUnion(transformedMeshAABB);
  }
  for (ui32 i = 0; i < (ui32)currentNode.childIndices.size(); i++)
  {
    computeSceneAABB(scene, accuAABB, currentNode.childIndices[i], accuTransformation);
  }
}

void SceneGraphFactory::createTextures(
    const std::unordered_map<std::filesystem::path, ui32>& textureFileNameToTextureIndex,
    std::filesystem::path parentPath, const ComPtr<ID3D12Device>& device,
    const ComPtr<ID3D12CommandQueue>& commandQueue, Scene& outputScene)
{
  (void)textureFileNameToTextureIndex;
  (void)parentPath;
  (void)device;
  (void)commandQueue;
  (void)outputScene;
  // Assignment 9
}

void SceneGraphFactory::createMaterials(aiScene const* const                            inputScene,
                                        std::unordered_map<std::filesystem::path, ui32> textureFileNameToTextureIndex,
                                        const ComPtr<ID3D12Device>& device, Scene& outputScene)
{
  (void)inputScene;
  (void)textureFileNameToTextureIndex;
  (void)device;
  (void)outputScene;
  // Assignment 7
  // Assignment 9
  // Assignment 10
}

} // namespace gims
