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
      const ui32v3 faceIndices = ui32v3(currentFace.mIndices[0], currentFace.mIndices[1], currentFace.mIndices[2]);
      result.emplace_back(faceIndices);
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
  // TODO: check if hasNormals, TextCoords etc.

  for (ui32 i = 0; i < inputScene->mNumMeshes; i++)
  {
    const aiMesh* currentMesh = inputScene->mMeshes[i];

    // get positions and normals
    const ui32         numVertices = currentMesh->mNumVertices;
    std::vector<f32v3> positions(numVertices);
    std::vector<f32v3> normals(numVertices);
    std::vector<f32v3> textureCoords(numVertices);
    for (ui32 n = 0; n < numVertices; n++)
    {
      const aiVector3D& currentPos      = currentMesh->mVertices[n];
      const aiVector3D& currentNormal   = currentMesh->mNormals[n];
      const aiVector3D& currentTexCoord = currentMesh->mTextureCoords[0][n]; // does this make sense?
      positions.emplace_back(currentPos.x, currentPos.y, currentPos.z);
      normals.emplace_back(currentNormal.x, currentNormal.y, currentNormal.z);
      textureCoords.emplace_back(currentTexCoord.x, currentTexCoord.y, currentTexCoord.z);
    }

    // get triangle indices
    const std::vector<ui32v3> indexBuffer     = getTriangleIndicesFromAiMesh(currentMesh);
    const ui32                indexBufferSize = static_cast<ui32>(indexBuffer.size());

    // create internal mesh
    TriangleMeshD3D12 createdMesh = TriangleMeshD3D12::TriangleMeshD3D12(
        positions.data(), normals.data(), textureCoords.data(), numVertices, indexBuffer.data(), indexBufferSize,
        currentMesh->mMaterialIndex, device, commandQueue);

    outputScene.m_meshes.emplace_back(createdMesh);
  }
}

ui32 SceneGraphFactory::createNodes(aiScene const* const inputScene, Scene& outputScene, aiNode const* const inputNode)
{
  (void)inputScene;
  (void)outputScene;
  (void)inputNode;
  // Assignment 4
  return 0;
}

void SceneGraphFactory::computeSceneAABB(Scene& scene, AABB& aabb, ui32 nodeIdx, f32m4 transformation)
{
  (void)transformation;
  (void)scene;
  (void)aabb;
  if (nodeIdx >= scene.getNumberOfNodes())
  {
    return;
  }
  // Assignment 5
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
