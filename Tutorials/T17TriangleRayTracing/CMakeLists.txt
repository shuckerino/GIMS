include("../../CreateApp.cmake")
set(SOURCES "./src/RayTracingRenderer.cpp" "./include/RayTracingRenderer.hpp" "./shaders/hlsl.h"
			#[["./DXRHelpers/DXRHelper.h"
			"./DXRHelpers/nv_helpers_dx12/BottomLevelASGenerator.h"
			"./DXRHelpers/nv_helpers_dx12/BottomLevelASGenerator.cpp"
			"./DXRHelpers/nv_helpers_dx12/RaytracingPipelineGenerator.h"
			"./DXRHelpers/nv_helpers_dx12/RaytracingPipelineGenerator.cpp"
			"./DXRHelpers/nv_helpers_dx12/RootSignatureGenerator.h"
			"./DXRHelpers/nv_helpers_dx12/RootSignatureGenerator.cpp"
			"./DXRHelpers/nv_helpers_dx12/ShaderBindingTableGenerator.h"
			"./DXRHelpers/nv_helpers_dx12/ShaderBindingTableGenerator.cpp"
			"./DXRHelpers/nv_helpers_dx12/TopLevelASGenerator.h" "./DXRHelpers/nv_helpers_dx12/TopLevelASGenerator.cpp"]])
set(SHADERS "./shaders/Triangle.hlsl")
create_app(T17TriangleRayTracing "${SOURCES}" "${SHADERS}")

