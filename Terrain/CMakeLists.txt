add_executable(Terrain Terrain.cpp TerrainMesh.cpp Heightfield.cpp)
target_link_libraries(Terrain PRIVATE SampleApp glfw)
target_compile_definitions(Terrain PRIVATE TERRAIN_ASSET_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/Assets")
