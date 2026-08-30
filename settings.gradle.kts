val maplibreRepositoryUrl = providers.gradleProperty("maplibreRepositoryUrl")
    .orElse(providers.environmentVariable("MAPLIBRE_REPOSITORY_URL"))

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
        if (maplibreRepositoryUrl.isPresent) {
            maven {
                name = "maplibreDevelopment"
                url = uri(maplibreRepositoryUrl.get())
            }
        }
    }
}

rootProject.name = "maplibre-native-plugin-template"
include(":plugins:fill-extrusion-shadows")
include(":plugins:gltf-layer")
include(":plugins:heatmap-layer")
include(":plugins:hillshade-layer")
include(":plugins:rectangle-layer")
include(":examples:android-app:app")
