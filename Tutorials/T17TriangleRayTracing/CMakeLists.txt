include("../../CreateApp.cmake")
set(SOURCES "./src/RayTracingRenderer.cpp" "./include/RayTracingRenderer.hpp" "./shaders/hlsl.h")
set(SHADERS "./shaders/Triangle.hlsl")
create_app(T17TriangleRayTracing "${SOURCES}" "${SHADERS}")

