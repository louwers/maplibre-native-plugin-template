cmake_minimum_required(VERSION 3.22.1)
project(rectangle_layer LANGUAGES C CXX)

if(NOT DEFINED MLN_RECTANGLE_PLUGIN_VERSION)
    set(MLN_RECTANGLE_PLUGIN_VERSION "0.1.0-local")
endif()
find_package(android-plugin-api REQUIRED CONFIG)

add_library(rectangle-layer SHARED
    ../../../../shared/cpp/rectangle_layer.cpp
    rectangle_layer_jni.cpp
)

target_compile_features(rectangle-layer PRIVATE cxx_std_20)
target_include_directories(rectangle-layer PRIVATE
    ../../../../shared/include
)
target_compile_definitions(rectangle-layer PRIVATE
    MLN_RECTANGLE_PLUGIN_VERSION="${MLN_RECTANGLE_PLUGIN_VERSION}"
)
target_link_libraries(rectangle-layer PRIVATE android-plugin-api::plugin_api android log)
