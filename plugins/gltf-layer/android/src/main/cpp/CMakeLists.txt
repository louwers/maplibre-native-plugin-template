cmake_minimum_required(VERSION 3.22.1)
project(gltf_layer LANGUAGES C CXX)

if(NOT DEFINED MLN_GLTF_PLUGIN_VERSION)
    set(MLN_GLTF_PLUGIN_VERSION "0.1.0-local")
endif()
find_package(android-plugin-api REQUIRED CONFIG)

add_library(gltf-layer SHARED
    ../../../../shared/cpp/gltf_layer.cpp
    ../../../../../../vendor/tinygltf/tiny_gltf.cc
    gltf_layer_jni.cpp
)

target_compile_features(gltf-layer PRIVATE cxx_std_20)
target_include_directories(gltf-layer PRIVATE
    ../../../../shared/include
    ../../../../../../vendor/tinygltf
)
target_compile_definitions(gltf-layer PRIVATE
    MLN_GLTF_PLUGIN_VERSION="${MLN_GLTF_PLUGIN_VERSION}"
)
target_link_libraries(gltf-layer PRIVATE android-plugin-api::plugin_api android log)
