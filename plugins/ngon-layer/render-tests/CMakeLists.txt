set(plugin_root "${CMAKE_CURRENT_LIST_DIR}/..")

add_executable(ngon-unit-tests
    "${plugin_root}/tests/ngon.test.cpp"
    "${plugin_root}/shared/cpp/ngon_layer.cpp"
)
target_compile_features(ngon-unit-tests PRIVATE cxx_std_20)
target_include_directories(ngon-unit-tests PRIVATE
    "${plugin_root}/shared/include" "${MAPLIBRE_NATIVE_SOURCE_DIR}/include")
add_test(NAME ngon-unit-tests COMMAND ngon-unit-tests)

mln_add_plugin_render_tests(
    NAME ngon-layer
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/register.cpp"
        "${plugin_root}/shared/cpp/ngon_layer.cpp"
    INCLUDE_DIRECTORIES "${plugin_root}/shared/include"
    COMPILE_DEFINITIONS MLN_NGON_PLUGIN_VERSION="ci"
)
