#include "gltf_layer.hpp"
#include "hillshade_layer.hpp"
#include "rectangle_layer.hpp"
#include "shadow_renderer.hpp"

#include <mln/render_test.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace fs = std::filesystem;

using RegisterFunction = mln_plugin_status (*)(mln_plugin_register_function_v1, char*, size_t);

struct PluginTestSuite {
    std::string_view directory;
    RegisterFunction registerPlugin;
};

std::vector<PluginTestSuite> pluginTestSuites() {
    std::vector<PluginTestSuite> suites{
        {"fill-extrusion-shadows", &mln_fill_extrusion_shadows_register},
        {"gltf-layer", &mln_gltf_layer_register},
        {"hillshade-layer", &mln_hillshade_layer_register},
        {"rectangle-layer", &mln_rectangle_layer_register},
    };
    return suites;
}

bool isRepositoryRoot(const fs::path& path) {
    std::error_code error;
    return fs::is_directory(path / "plugins", error) && fs::is_regular_file(path / "MODULE.bazel", error);
}

std::optional<fs::path> findRepositoryRoot(fs::path candidate) {
    std::error_code error;
    candidate = fs::absolute(std::move(candidate), error);
    if (error) return std::nullopt;
    while (!candidate.empty()) {
        if (isRepositoryRoot(candidate)) return candidate.lexically_normal();
        const auto parent = candidate.parent_path();
        if (parent == candidate) break;
        candidate = parent;
    }
    return std::nullopt;
}

std::optional<fs::path> repositoryRoot(const std::optional<fs::path>& requested, const char* executable) {
    if (requested) return findRepositoryRoot(*requested);

    if (const char* workspace = std::getenv("BUILD_WORKSPACE_DIRECTORY")) {
        if (auto root = findRepositoryRoot(workspace)) return root;
    }
    if (const char* testSourceDirectory = std::getenv("TEST_SRCDIR")) {
        if (const char* testWorkspace = std::getenv("TEST_WORKSPACE")) {
            if (auto root = findRepositoryRoot(fs::path(testSourceDirectory) / testWorkspace)) return root;
        }
    }
    if (auto root = findRepositoryRoot(fs::current_path())) return root;
    if (executable && *executable) {
        if (auto root = findRepositoryRoot(fs::path(executable).parent_path())) return root;
    }
    return std::nullopt;
}

bool registerPlugins(const std::vector<PluginTestSuite>& suites) {
    for (const auto& suite : suites) {
        char error[512]{};
        const auto status = suite.registerPlugin(&mln_plugin_register_v1, error, sizeof(error));
        if (status != MLN_PLUGIN_STATUS_OK && status != MLN_PLUGIN_STATUS_ALREADY_REGISTERED) {
            std::cerr << "Unable to register plugin for " << suite.directory << ": " << error << '\n';
            return false;
        }
    }
    return true;
}

std::vector<fs::path> discoverManifests(const fs::path& root) {
    std::vector<fs::path> manifests;
    std::error_code error;
    const fs::directory_iterator end;
    for (fs::directory_iterator plugin(root / "plugins", error); !error && plugin != end; plugin.increment(error)) {
        if (!plugin->is_directory(error)) continue;
        const auto manifest = plugin->path() / "render-tests" / "manifest.json";
        if (fs::is_regular_file(manifest, error)) manifests.push_back(manifest.lexically_normal());
    }
    std::sort(manifests.begin(), manifests.end());
    return manifests;
}

int runManifest(const char* executable, const std::vector<std::string>& forwarded, const fs::path& manifest) {
    std::vector<std::string> arguments;
    arguments.reserve(forwarded.size() + 3);
    arguments.emplace_back(executable ? executable : "plugin-render-tests");
    arguments.insert(arguments.end(), forwarded.begin(), forwarded.end());
    arguments.emplace_back("--manifestPath");
    arguments.push_back(manifest.string());

    std::vector<char*> rawArguments;
    rawArguments.reserve(arguments.size());
    for (auto& argument : arguments) rawArguments.push_back(argument.data());

    std::cout << "\n=== Plugin render tests: " << manifest.parent_path().parent_path().filename().string() << " ===\n";
    return mln::runRenderTests(static_cast<int>(rawArguments.size()), rawArguments.data(), {});
}

} // namespace

int main(int argc, char** argv) {
    std::optional<fs::path> requestedRoot;
    std::optional<fs::path> explicitManifest;
    bool listOnly = false;
    std::vector<std::string> forwarded;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument{argv[i]};
        if (argument == "--plugin-test-root") {
            if (++i >= argc) {
                std::cerr << "--plugin-test-root requires a path\n";
                return 64;
            }
            requestedRoot = argv[i];
        } else if (argument.starts_with("--plugin-test-root=")) {
            requestedRoot = std::string(argument.substr(std::string_view{"--plugin-test-root="}.size()));
        } else if (argument == "--list-plugin-tests") {
            listOnly = true;
        } else if (argument == "--manifestPath" || argument == "-p") {
            if (++i >= argc) {
                std::cerr << argument << " requires a path\n";
                return 64;
            }
            explicitManifest = argv[i];
        } else if (argument.starts_with("--manifestPath=")) {
            explicitManifest = std::string(argument.substr(std::string_view{"--manifestPath="}.size()));
        } else if (argument.starts_with("-p=")) {
            explicitManifest = std::string(argument.substr(3));
        } else {
            forwarded.emplace_back(argument);
        }
    }

    const auto suites = pluginTestSuites();
    if (!registerPlugins(suites)) return 4;

    std::vector<fs::path> manifests;
    if (explicitManifest) {
        std::error_code error;
        const auto absoluteManifest = fs::absolute(*explicitManifest, error);
        if (!error) manifests.push_back(absoluteManifest.lexically_normal());
    } else {
        const auto root = repositoryRoot(requestedRoot, argc > 0 ? argv[0] : nullptr);
        if (!root) {
            std::cerr << "Unable to locate the plugin-template repository. Use --plugin-test-root <path>.\n";
            return 66;
        }
        manifests = discoverManifests(*root);
    }

    if (manifests.empty()) {
        std::cerr << "No plugin render-test manifests were found.\n";
        return 66;
    }
    if (listOnly) {
        for (const auto& manifest : manifests) std::cout << manifest.string() << '\n';
        return 0;
    }

    int result = 0;
    for (const auto& manifest : manifests) {
        result = std::max(result, runManifest(argc > 0 ? argv[0] : nullptr, forwarded, manifest));
    }
    return result;
}
