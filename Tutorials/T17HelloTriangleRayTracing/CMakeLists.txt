include("../../CreateApp.cmake")
set(SOURCES "./src/TriangleApp.cpp")
set(SHADERS "./shaders/Triangle.hlsl")
create_app(T17HelloTriangleRayTracing "${SOURCES}" "${SHADERS}")