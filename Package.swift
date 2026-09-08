// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "MapLibreNativePlugins",
    platforms: [
        .iOS(.v14),
        .macOS(.v10_15),
    ],
    products: [
        .library(
            name: "GltfLayer",
            targets: ["GltfLayer"]
        ),
        .library(
            name: "HeatmapLayer",
            targets: ["HeatmapLayer"]
        ),
        .library(
            name: "HillshadeLayer",
            targets: ["HillshadeLayer"]
        ),
        .library(
            name: "RectangleLayer",
            targets: ["RectangleLayer"]
        ),
        .library(
            name: "NgonLayer",
            targets: ["NgonLayer"]
        ),
    ],
    targets: [
        .target(
            name: "MapLibrePluginApi",
            path: "MapLibrePluginApi",
            sources: ["src/plugin_api_anchor.c"],
            publicHeadersPath: "include"
        ),
        .target(
            name: "TinyGLTF",
            path: "vendor/tinygltf",
            exclude: [
                "CMakeLists.txt", "LICENSE", "Makefile", "README.md", "appveyor.yml", "cmake", "examples",
                "examples.bat", "experimental", "loader_example.cc", "models", "premake5.lua", "test_runner.py",
                "tests", "vcsetup.bat", "wasm",
            ],
            sources: ["tiny_gltf.cc"],
            publicHeadersPath: "."
        ),
        .target(
            name: "GltfLayer",
            dependencies: ["MapLibrePluginApi", "TinyGLTF"],
            path: "plugins/gltf-layer",
            exclude: [
                "BUILD.bazel",
                "README.md",
                "android",
            ],
            sources: [
                "ios/src/GltfLayer.mm",
                "shared/cpp/gltf_layer.cpp",
            ],
            publicHeadersPath: "ios/include",
            cxxSettings: [
                .headerSearchPath("shared/include"),
                .define("MLN_GLTF_PLUGIN_VERSION", to: "\"0.1.0\""),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
            ]
        ),
        .target(
            name: "HeatmapLayer",
            dependencies: ["MapLibrePluginApi"],
            path: "plugins/heatmap-layer",
            exclude: [
                "BUILD.bazel",
                "README.md",
                "android",
                "render-tests",
            ],
            sources: [
                "ios/src/HeatmapLayer.mm",
                "shared/cpp/heatmap_layer.cpp",
            ],
            publicHeadersPath: "ios/include",
            cxxSettings: [
                .headerSearchPath("shared/include"),
                .define("MLN_HEATMAP_PLUGIN_VERSION", to: "\"0.1.0\""),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
            ]
        ),
        .target(
            name: "HillshadeLayer",
            dependencies: ["MapLibrePluginApi"],
            path: "plugins/hillshade-layer",
            exclude: [
                "BUILD.bazel",
                "README.md",
                "android",
                "render-tests",
            ],
            sources: [
                "ios/src/HillshadeLayer.mm",
                "shared/cpp/hillshade_layer.cpp",
            ],
            publicHeadersPath: "ios/include",
            cxxSettings: [
                .headerSearchPath("shared/include"),
                .define("MLN_HILLSHADE_PLUGIN_VERSION", to: "\"0.1.0\""),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
            ]
        ),
        .target(
            name: "RectangleLayer",
            dependencies: ["MapLibrePluginApi"],
            path: "plugins/rectangle-layer",
            exclude: [
                "BUILD.bazel",
                "README.md",
                "android",
                "render-tests",
            ],
            sources: [
                "ios/src/RectangleLayer.mm",
                "shared/cpp/rectangle_layer.cpp",
            ],
            publicHeadersPath: "ios/include",
            cxxSettings: [
                .headerSearchPath("shared/include"),
                .define("MLN_RECTANGLE_PLUGIN_VERSION", to: "\"0.1.0\""),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
            ]
        ),
        .target(
            name: "NgonLayer",
            dependencies: ["MapLibrePluginApi"],
            path: "plugins/ngon-layer",
            exclude: [
                "BUILD.bazel",
                "README.md",
                "android",
                "render-tests", "scripts", "tests", "release.json", "build.gradle.kts",
            ],
            sources: [
                "ios/src/NgonLayer.mm",
                "shared/cpp/ngon_layer.cpp",
            ],
            publicHeadersPath: "ios/include",
            cxxSettings: [
                .headerSearchPath("shared/include"),
                .define("MLN_NGON_PLUGIN_VERSION", to: "\"0.1.0\""),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
            ]
        ),
    ],
    cxxLanguageStandard: .cxx20
)
