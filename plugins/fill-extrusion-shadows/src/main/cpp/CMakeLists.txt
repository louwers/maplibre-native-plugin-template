cmake_minimum_required(VERSION 3.22.1)
project(fill_extrusion_shadows LANGUAGES C CXX)

if(NOT DEFINED MLN_SHADOW_PLUGIN_VERSION)
    set(MLN_SHADOW_PLUGIN_VERSION "0.1.0-local")
endif()

find_package(android-plugin-api REQUIRED CONFIG)

add_library(fill-extrusion-shadows SHARED
    fill_extrusion_shadows.cpp
    shadow_opengl.cpp
    shadow_vulkan.cpp
)

target_compile_features(fill-extrusion-shadows PRIVATE cxx_std_20)
target_compile_definitions(fill-extrusion-shadows PRIVATE
    VK_NO_PROTOTYPES
    MLN_SHADOW_PLUGIN_VERSION="${MLN_SHADOW_PLUGIN_VERSION}"
)
target_link_libraries(fill-extrusion-shadows PRIVATE
    android-plugin-api::plugin_api
    android
    log
    GLESv3
)
