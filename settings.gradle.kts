// Android build of this plugin. All logic lives in maplibre-native-plugins (tools/);
// the plugin is configured by plugin.json. Build with tools/bin/plugin android-aar.
pluginManagement {
    includeBuild("tools/gradle")
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

plugins {
    id("org.maplibre.native-plugin.settings")
}
