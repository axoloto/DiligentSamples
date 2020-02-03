cmake_minimum_required (VERSION 3.6)

project(TerrainRenderer CXX)

set(SOURCE
    src/TerrainRenderer.cpp
    src/Terrain/EarthMesh.cpp
)

set(INCLUDE
    src/TerrainRenderer.h
    src/Terrain/EarthMesh.h
)

set(SHADERS
    assets/shaders/earth.psh
    assets/shaders/earth.vsh
    assets/shaders/light.psh
    assets/shaders/light.vsh
)

set(TEXTURES
    assets/Terrain/height.png
    assets/Terrain/texture.png
)


add_sample_app("TerrainRenderer" "DiligentSamples/Samples" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${TEXTURES}")

set_source_files_properties(${SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION "Resources/shaders"
)
set_source_files_properties(${TEXTURES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "Terrain"
    MACOSX_PACKAGE_LOCATION "Resources/Terrain"
)

target_include_directories(TerrainRenderer
PRIVATE
    src
    src/Terrain
)

target_link_libraries(TerrainRenderer
PRIVATE
    Diligent-GraphicsAccessories
    Diligent-TextureLoader
)

source_group("assets\\shaders" FILES ${SHADERS})
source_group("assets\\Terrain" FILES ${TEXTURES})

