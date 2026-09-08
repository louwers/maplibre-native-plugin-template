cmake_minimum_required(VERSION 3.22.1)
project(ngon_layer LANGUAGES C CXX)

if(NOT DEFINED MLN_NGON_PLUGIN_VERSION)
    set(MLN_NGON_PLUGIN_VERSION "0.1.0-local")
endif()
find_package(android-plugin-api REQUIRED CONFIG)

add_library(ngon-layer SHARED
    ../../../../shared/cpp/ngon_layer.cpp
    ngon_layer_jni.cpp
)

target_compile_features(ngon-layer PRIVATE cxx_std_20)
target_include_directories(ngon-layer PRIVATE
    ../../../../shared/include
)
target_compile_definitions(ngon-layer PRIVATE
    MLN_NGON_PLUGIN_VERSION="${MLN_NGON_PLUGIN_VERSION}"
)
target_link_libraries(ngon-layer PRIVATE android-plugin-api::plugin_api android log)
