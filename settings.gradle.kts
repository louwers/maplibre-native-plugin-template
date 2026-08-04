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
        val pluginRepository = providers.gradleProperty("maplibrePluginRepository")
            .orElse(providers.environmentVariable("REPOSILITE_URL"))
        if (pluginRepository.isPresent) {
            val repositoryUri = uri(pluginRepository.get())
            val repositoryUsername = providers.gradleProperty("reposiliteUsername")
                .orElse(providers.environmentVariable("REPOSILITE_USERNAME"))
            val repositoryPassword = providers.gradleProperty("reposilitePassword")
                .orElse(providers.environmentVariable("REPOSILITE_PASSWORD"))
            maven {
                name = "maplibrePluginSnapshots"
                url = repositoryUri
                if (repositoryUri.scheme in setOf("http", "https") &&
                    repositoryUsername.isPresent && repositoryPassword.isPresent
                ) {
                    credentials {
                        username = repositoryUsername.get()
                        password = repositoryPassword.get()
                    }
                }
                mavenContent { snapshotsOnly() }
            }
        }
    }
}

rootProject.name = "maplibre-native-plugin-template"
include(":plugins:fill-extrusion-shadows")
include(":examples:android-app:app")
