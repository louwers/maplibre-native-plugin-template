.target(
            name: "HeatmapLayer",
            dependencies: [.product(name: "MapLibrePluginApi", package: "maplibre-native")],
            path: "plugins/heatmap-layer",
            exclude: [
                "BUILD.bazel",
                "README.md",
                "plugin.json", "swift-targets.swift", "examples",
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
