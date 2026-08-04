// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "MapLibreNativePlugins",
    platforms: [
        .iOS(.v14),
    ],
    products: [
        .library(
            name: "FillExtrusionShadows",
            targets: ["FillExtrusionShadows"]
        ),
    ],
    targets: [
        .target(
            name: "FillExtrusionShadows",
            path: "plugins/fill-extrusion-shadows",
            exclude: [
                "BUILD.bazel",
                "android",
            ],
            sources: [
                "ios/src/FillExtrusionShadows.mm",
                "ios/src/shadow_metal.mm",
                "shared/cpp/shadow_plugin.cpp",
            ],
            publicHeadersPath: "ios/include",
            cxxSettings: [
                .headerSearchPath("shared/include"),
                .define("MLN_SHADOW_IOS", to: "1"),
                .define("MLN_SHADOW_PLUGIN_VERSION", to: "\"0.0.2\""),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
                .linkedFramework("Metal"),
            ]
        ),
    ],
    cxxLanguageStandard: .cxx20
)
