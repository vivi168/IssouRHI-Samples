add_executable(HelloCube HelloCube.cpp Camera.cpp)
target_link_libraries(HelloCube PRIVATE common DirectXMath glfw IssouRHI)
