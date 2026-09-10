.target(
            name: "RectangleLayer",
            dependencies: ["MapLibrePluginApi"],
            path: "plugins/rectangle-layer",
            exclude: [
                "BUILD.bazel",
                "README.md",
                "plugin.json", "swift-targets.swift", "examples",
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
