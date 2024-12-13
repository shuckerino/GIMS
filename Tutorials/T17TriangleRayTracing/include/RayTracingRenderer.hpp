#pragma once
//#include <dxcapi.h>
#include <gimslib/d3d/DX12App.hpp>
#include <gimslib/d3d/DX12Util.hpp>
#include <gimslib/d3d/UploadHelper.hpp>
#include <gimslib/dbg/HrException.hpp>
#include <gimslib/types.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
//W#include "../shaders/hlsl.h"

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

class GpuUploadBuffer
{
public:
  ComPtr<ID3D12Resource> GetResource()
  {
    return m_resource;
  }

protected:
  ComPtr<ID3D12Resource> m_resource;

  GpuUploadBuffer()
  {
  }
  ~GpuUploadBuffer()
  {
    if (m_resource.Get())
    {
      m_resource->Unmap(0, nullptr);
    }
  }

  void Allocate(ID3D12Device* device, UINT bufferSize, LPCWSTR resourceName = nullptr)
  {
    auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    throwIfFailed(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                  IID_PPV_ARGS(&m_resource)));
    m_resource->SetName(resourceName);
  }

  uint8_t* MapCpuWriteOnly()
  {
    uint8_t* mappedData;
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
    throwIfFailed(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
    return mappedData;
  }
};

// Shader record = {{Shader ID}, {RootArguments}}
class ShaderRecord
{
public:
  ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize)
      : shaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
  {
  }

  ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize, void* pLocalRootArguments,
               UINT localRootArgumentsSize)
      : shaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
      , localRootArguments(pLocalRootArguments, localRootArgumentsSize)
  {
  }

  void CopyTo(void* dest) const
  {
    uint8_t* byteDest = static_cast<uint8_t*>(dest);
    memcpy(byteDest, shaderIdentifier.ptr, shaderIdentifier.size);
    if (localRootArguments.ptr)
    {
      memcpy(byteDest + shaderIdentifier.size, localRootArguments.ptr, localRootArguments.size);
    }
  }

  struct PointerWithSize
  {
    void* ptr;
    UINT  size;

    PointerWithSize()
        : ptr(nullptr)
        , size(0)
    {
    }
    PointerWithSize(void* _ptr, UINT _size)
        : ptr(_ptr)
        , size(_size) {};
  };
  PointerWithSize shaderIdentifier;
  PointerWithSize localRootArguments;
};

inline UINT Align(UINT size, UINT alignment)
{
  return (size + (alignment - 1)) & ~(alignment - 1);
}

// Shader table = {{ ShaderRecord 1}, {ShaderRecord 2}, ...}
class ShaderTable : public GpuUploadBuffer
{
  uint8_t* m_mappedShaderRecords;
  UINT     m_shaderRecordSize;

  // Debug support
  std::wstring              m_name;
  std::vector<ShaderRecord> m_shaderRecords;

  ShaderTable()
  {
  }

public:
  ShaderTable(ID3D12Device* device, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName)
      : m_name(resourceName)
  {
    m_shaderRecordSize = Align(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    m_shaderRecords.reserve(numShaderRecords);
    UINT bufferSize = numShaderRecords * m_shaderRecordSize;
    Allocate(device, bufferSize, resourceName);
    m_mappedShaderRecords = MapCpuWriteOnly();
  }

  void push_back(const ShaderRecord& shaderRecord)
  {
    throwIfZero(m_shaderRecords.size() < m_shaderRecords.capacity());
    m_shaderRecords.push_back(shaderRecord);
    shaderRecord.CopyTo(m_mappedShaderRecords);
    m_mappedShaderRecords += m_shaderRecordSize;
  }

  UINT GetShaderRecordSize()
  {
    return m_shaderRecordSize;
  }

  // Pretty-print the shader records.
  void DebugPrint(std::unordered_map<void*, std::wstring> shaderIdToStringMap)
  {
    std::wstringstream wstr;
    wstr << L"|--------------------------------------------------------------------\n";
    wstr << L"|Shader table - " << m_name.c_str() << L": " << m_shaderRecordSize << L" | "
         << m_shaderRecords.size() * m_shaderRecordSize << L" bytes\n";

    for (UINT i = 0; i < m_shaderRecords.size(); i++)
    {
      wstr << L"| [" << i << L"]: ";
      wstr << shaderIdToStringMap[m_shaderRecords[i].shaderIdentifier.ptr] << L", ";
      wstr << m_shaderRecords[i].shaderIdentifier.size << L" + " << m_shaderRecords[i].localRootArguments.size
           << L" bytes \n";
    }
    wstr << L"|--------------------------------------------------------------------\n";
    wstr << L"\n";
    OutputDebugStringW(wstr.str().c_str());
  }
};





class RayTracingRenderer : public DX12App
{
public:
  RayTracingRenderer(const DX12AppConfig createInfo);

  //struct Vertex
  //{
  //  f32v3 position;
  //};
  struct Vertex
  {
    float v1, v2, v3;
  };


  ID3D12Device5* getRTDevice()
  {
    return m_dxrDevice.Get();
  }

  /// <summary>
  /// Called whenever a new frame is drawn.
  /// </summary>
  virtual void onDraw();

  /// <summary>
  /// Draw UI onto of rendered result.
  /// </summary>
  virtual void onDrawUI();

private:
  // DirectX ray tracing interfaces
  ComPtr<ID3D12Device5>              m_dxrDevice;
  //ComPtr<ID3D12GraphicsCommandList4> m_dxrCommandList;
  std::vector<ComPtr<ID3D12GraphicsCommandList4>> m_dxrCommandLists;
  ComPtr<ID3D12StateObject>          m_dxrStateObject;
  ComPtr<ID3D12CommandAllocator> m_dxrCommandAllocator;
  ComPtr<ID3D12CommandQueue>         m_dxrCommandQueue;

  // Acceleration structure
  ComPtr<ID3D12Resource> m_topLevelAS;
  ComPtr<ID3D12Resource> m_bottomLevelAS;

  // Root signatures
  ComPtr<ID3D12RootSignature> m_globalRootSignature;
  ComPtr<ID3D12RootSignature> m_LocalRootSignature;

  // Ray tracing output
  ComPtr<ID3D12Resource>      m_raytracingOutput;
  D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
  UINT                        m_raytracingOutputResourceUAVDescriptorHeapIndex;

  ComPtr<ID3D12StateObject> m_rtStateObject;

  // Descriptor heap
  ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
  ui32                         m_descriptorsAllocated;
  ui32                         m_descriptorSize;

  // Shader tables
  static const wchar_t*  c_hitGroupName;
  static const wchar_t*  c_raygenShaderName;
  static const wchar_t*  c_closestHitShaderName;
  static const wchar_t*  c_missShaderName;
  ComPtr<ID3D12Resource> m_missShaderTable;
  ComPtr<ID3D12Resource> m_hitGroupShaderTable;
  ComPtr<ID3D12Resource> m_rayGenShaderTable;

  // Raytracing scene
  struct Viewport
  {
    float left;
    float top;
    float right;
    float bottom;
  };
  struct RayGenConstantBuffer
  {
    Viewport viewport;
    Viewport stencil;
  };
  RayGenConstantBuffer m_rayGenCB;

  // Geometry
  typedef UINT16 Index;
  ui32                     m_vertexBufferSize; //! Vertex buffer size in bytes.
  ui32                     m_indexBufferSize;  //! Index buffer size in bytes.
  ComPtr<ID3D12Resource>   m_indexBuffer;
  ComPtr<ID3D12Resource>   m_vertexBuffer;
  //D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
  //D3D12_INDEX_BUFFER_VIEW  m_indexBufferView;

  // Init functions
  bool isRayTracingSupported();
  void createRayTracingResources();
  void createRayTracingInterfaces();
  void createRootSignatures();
  void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);
  void createRayTracingPipeline();
  void createShaderTables();
  void createDescriptorHeap();
  void createOutputResource();

  // Geometry functions
  void createGeometry();

  // Acceleration structure functions
  void                           createAccelerationStructures();
  D3D12_RAYTRACING_GEOMETRY_DESC createGeometryDescription();

  // Helper functions
  void AllocateUAVBuffer(ui64 bufferSize, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initialResourceState,
                         const wchar_t* resourceName);
  void DoRayTracing();
  void CopyRaytracingOutputToBackbuffer();
  struct AccelerationStructureBuffers
  {
    ComPtr<ID3D12Resource> pScratch;      // Scratch memory for AS builder
    ComPtr<ID3D12Resource> pResult;       // Where the AS is
    ComPtr<ID3D12Resource> pInstanceDesc; // Hold the matrices of the instances
  };
};
