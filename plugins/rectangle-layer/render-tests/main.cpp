#include "rectangle_layer.hpp"

#include <mbgl/render_test.hpp>

#include <cstdio>

int main(int argc, char** argv) {
    char error[512]{};
    const auto status = mln_rectangle_layer_register(&mln_plugin_register_v1, error, sizeof(error));
    if (status != MLN_PLUGIN_STATUS_OK && status != MLN_PLUGIN_STATUS_ALREADY_REGISTERED) {
        std::fprintf(stderr, "Unable to register rectangle plugin: %s\n", error);
        return 4;
    }
    return mbgl::runRenderTests(argc, argv, {});
}
