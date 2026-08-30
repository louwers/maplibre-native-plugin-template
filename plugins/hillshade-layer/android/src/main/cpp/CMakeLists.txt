cmake_minimum_required(VERSION 3.22.1)
project(hillshade_layer LANGUAGES C CXX)

if(NOT DEFINED MLN_HILLSHADE_PLUGIN_VERSION)
    set(MLN_HILLSHADE_PLUGIN_VERSION "0.1.0-local")
endif()
find_package(android-plugin-api REQUIRED CONFIG)

add_library(hillshade-layer SHARED
    ../../../../shared/cpp/hillshade_layer.cpp
    hillshade_layer_jni.cpp
)

target_compile_features(hillshade-layer PRIVATE cxx_std_20)
target_include_directories(hillshade-layer PRIVATE ../../../../shared/include)
target_compile_definitions(hillshade-layer PRIVATE
    MLN_HILLSHADE_PLUGIN_VERSION="${MLN_HILLSHADE_PLUGIN_VERSION}"
)
target_link_libraries(hillshade-layer PRIVATE android-plugin-api::plugin_api android log)
