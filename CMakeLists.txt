cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(maplibre-native-plugin-render-tests LANGUAGES C CXX)

set(MAPLIBRE_NATIVE_SOURCE_DIR "" CACHE PATH "Path to a plugin-enabled MapLibre Native checkout")
if(NOT MAPLIBRE_NATIVE_SOURCE_DIR)
    message(FATAL_ERROR "Set MAPLIBRE_NATIVE_SOURCE_DIR to a plugin-enabled MapLibre Native checkout")
endif()
get_filename_component(MAPLIBRE_NATIVE_SOURCE_DIR "${MAPLIBRE_NATIVE_SOURCE_DIR}" ABSOLUTE)
if(NOT EXISTS "${MAPLIBRE_NATIVE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "MAPLIBRE_NATIVE_SOURCE_DIR does not contain MapLibre Native")
endif()

if(NOT MLN_WITH_OPENGL AND NOT MLN_WITH_VULKAN)
    message(FATAL_ERROR "Plugin render tests require MLN_WITH_OPENGL=ON or MLN_WITH_VULKAN=ON")
endif()
if(MLN_WITH_OPENGL AND MLN_WITH_VULKAN)
    message(FATAL_ERROR "Select exactly one plugin render-test backend")
endif()

set(MLN_WITH_GLFW OFF CACHE BOOL "" FORCE)
set(MLN_WITH_WERROR OFF CACHE BOOL "" FORCE)
add_subdirectory("${MAPLIBRE_NATIVE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/maplibre-native")

add_executable(
    plugin-render-tests
    render-tests/main.cpp
    plugins/fill-extrusion-shadows/shared/cpp/shadow_plugin.cpp
    plugins/gltf-layer/shared/cpp/gltf_layer.cpp
    plugins/hillshade-layer/shared/cpp/hillshade_layer.cpp
    plugins/rectangle-layer/shared/cpp/rectangle_layer.cpp
    vendor/tinygltf/tiny_gltf.cc
)

target_compile_features(plugin-render-tests PRIVATE cxx_std_20)
target_compile_definitions(
    plugin-render-tests
    PRIVATE
        MLN_GLTF_PLUGIN_VERSION="ci"
        MLN_HILLSHADE_PLUGIN_VERSION="ci"
        MLN_RECTANGLE_PLUGIN_VERSION="ci"
        MLN_SHADOW_PLUGIN_VERSION="ci"
)
target_include_directories(
    plugin-render-tests
    PRIVATE
        plugins/fill-extrusion-shadows/shared/include
        plugins/gltf-layer/shared/include
        plugins/hillshade-layer/shared/include
        plugins/rectangle-layer/shared/include
        vendor/tinygltf
)
target_link_libraries(plugin-render-tests PRIVATE mbgl-render-test)

if(MLN_WITH_OPENGL)
    find_package(OpenGL REQUIRED)
    target_sources(plugin-render-tests PRIVATE plugins/fill-extrusion-shadows/shared/cpp/shadow_opengl.cpp)
    target_compile_definitions(plugin-render-tests PRIVATE MLN_SHADOW_OPENGL=1)
    target_link_libraries(plugin-render-tests PRIVATE OpenGL::GL)
elseif(MLN_WITH_VULKAN)
    target_sources(plugin-render-tests PRIVATE plugins/fill-extrusion-shadows/android/src/main/cpp/shadow_vulkan.cpp)
    target_compile_definitions(plugin-render-tests PRIVATE MLN_SHADOW_VULKAN=1 VK_NO_PROTOTYPES)
    target_include_directories(plugin-render-tests PRIVATE "${MAPLIBRE_NATIVE_SOURCE_DIR}/vendor/Vulkan-Headers/include")
endif()
