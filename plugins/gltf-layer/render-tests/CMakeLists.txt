set(plugin_root "${CMAKE_CURRENT_LIST_DIR}/..")

mln_add_plugin_render_tests(
    NAME gltf-layer
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/register.cpp"
        "${plugin_root}/shared/cpp/gltf_layer.cpp"
        "${PROJECT_SOURCE_DIR}/vendor/tinygltf/tiny_gltf.cc"
    INCLUDE_DIRECTORIES
        "${plugin_root}/shared/include"
        "${PROJECT_SOURCE_DIR}/vendor/tinygltf"
    COMPILE_DEFINITIONS MLN_GLTF_PLUGIN_VERSION="ci"
)
