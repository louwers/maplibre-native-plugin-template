pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "maplibre-native-plugin-template"
include(":plugins:fill-extrusion-shadows")
include(":plugins:gltf-layer")
include(":examples:android-app:app")
