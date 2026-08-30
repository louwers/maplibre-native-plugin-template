set(plugin_root "${CMAKE_CURRENT_LIST_DIR}/..")
set(plugin_sources
    "${CMAKE_CURRENT_LIST_DIR}/register.cpp"
    "${plugin_root}/shared/cpp/shadow_plugin.cpp"
)
set(plugin_definitions MLN_SHADOW_PLUGIN_VERSION="ci")
set(plugin_libraries)

if(MLN_WITH_OPENGL)
    find_package(OpenGL REQUIRED)
    list(APPEND plugin_sources "${plugin_root}/shared/cpp/shadow_opengl.cpp")
    list(APPEND plugin_definitions MLN_SHADOW_OPENGL=1)
    list(APPEND plugin_libraries OpenGL::GL)
elseif(MLN_WITH_VULKAN)
    list(APPEND plugin_sources "${plugin_root}/android/src/main/cpp/shadow_vulkan.cpp")
    list(APPEND plugin_definitions MLN_SHADOW_VULKAN=1 VK_NO_PROTOTYPES)
endif()

mln_add_plugin_render_tests(
    NAME fill-extrusion-shadows
    SOURCES ${plugin_sources}
    INCLUDE_DIRECTORIES
        "${plugin_root}/shared/include"
        "${MAPLIBRE_NATIVE_SOURCE_DIR}/vendor/Vulkan-Headers/include"
    COMPILE_DEFINITIONS ${plugin_definitions}
    LINK_LIBRARIES ${plugin_libraries}
)
