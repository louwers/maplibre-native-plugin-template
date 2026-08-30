set(plugin_root "${CMAKE_CURRENT_LIST_DIR}/..")

mln_add_plugin_render_tests(
    NAME hillshade-layer
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/register.cpp"
        "${plugin_root}/shared/cpp/hillshade_layer.cpp"
    INCLUDE_DIRECTORIES "${plugin_root}/shared/include"
    COMPILE_DEFINITIONS MLN_HILLSHADE_PLUGIN_VERSION="ci"
)
