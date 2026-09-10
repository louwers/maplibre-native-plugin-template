.target(
            name: "HillshadeLayer",
            dependencies: ["MapLibrePluginApi"],
            path: "plugins/hillshade-layer",
            exclude: [
                "BUILD.bazel",
                "README.md",
                "plugin.json", "swift-targets.swift", "examples",
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
