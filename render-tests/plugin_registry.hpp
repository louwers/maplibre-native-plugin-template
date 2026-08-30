#pragma once

#include <mln/plugin/plugin_api.h>

#include <string_view>
#include <vector>

namespace mln::plugin::test {

using RegisterFunction = mln_plugin_status (*)(mln_plugin_register_function_v1, char*, size_t);

struct PluginRegistration {
    RegisterFunction function;
    std::string_view symbol;
};

inline std::vector<PluginRegistration>& mutablePluginRegistrations() {
    static std::vector<PluginRegistration> registrations;
    return registrations;
}

class AutoRegistration {
public:
    AutoRegistration(RegisterFunction function, std::string_view symbol) {
        mutablePluginRegistrations().push_back({function, symbol});
    }
};

inline const std::vector<PluginRegistration>& pluginRegistrations() {
    return mutablePluginRegistrations();
}

} // namespace mln::plugin::test

#define MLN_PLUGIN_RENDER_TEST_REGISTER(function)                                                        \
    namespace {                                                                                          \
    const ::mln::plugin::test::AutoRegistration mlnPluginRenderTestRegistration{&(function), #function}; \
    }
