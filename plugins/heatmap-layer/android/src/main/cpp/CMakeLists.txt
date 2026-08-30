cmake_minimum_required(VERSION 3.22.1)
project(heatmap_layer LANGUAGES C CXX)

if(NOT DEFINED MLN_HEATMAP_PLUGIN_VERSION)
    set(MLN_HEATMAP_PLUGIN_VERSION "0.1.0-local")
endif()
find_package(android-plugin-api REQUIRED CONFIG)

add_library(heatmap-layer SHARED
    ../../../../shared/cpp/heatmap_layer.cpp
    heatmap_layer_jni.cpp
)

target_compile_features(heatmap-layer PRIVATE cxx_std_20)
target_include_directories(heatmap-layer PRIVATE ../../../../shared/include)
target_compile_definitions(heatmap-layer PRIVATE
    MLN_HEATMAP_PLUGIN_VERSION="${MLN_HEATMAP_PLUGIN_VERSION}"
)
target_link_libraries(heatmap-layer PRIVATE android-plugin-api::plugin_api android log)
