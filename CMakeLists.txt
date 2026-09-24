cmake_minimum_required(VERSION 3.30)
project(IssouRHISample)

set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_library(common INTERFACE)
target_compile_features(common INTERFACE cxx_std_23)
target_compile_definitions(common INTERFACE SAMPLE_SHADER_DIRECTORY="${CMAKE_BINARY_DIR}/Shaders")
if(MSVC)
    target_compile_options(common INTERFACE /W4 /permissive-)
    target_compile_definitions(common INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
endif()

add_subdirectory(third_party/glfw)
add_subdirectory(third_party/IssouRHI)
add_subdirectory(third_party/directxmath)

add_subdirectory(HelloTriangle)
add_subdirectory(HelloCube)
