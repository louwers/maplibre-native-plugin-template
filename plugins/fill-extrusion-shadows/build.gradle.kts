import org.gradle.api.artifacts.type.ArtifactTypeDefinition

plugins {
    id("com.android.library")
    id("maven-publish")
}

val maplibreVersion = providers.gradleProperty("maplibreVersion")
val maplibreJavaApiVersion = providers.gradleProperty("maplibreJavaApiVersion")
val pluginVersion = providers.gradleProperty("pluginVersion")
val pluginGroup = providers.gradleProperty("pluginGroup").orElse("org.maplibre.plugins")
val pluginAbis = providers.gradleProperty("maplibrePluginAbis").orNull

val maplibreJavaApi by configurations.creating {
    isCanBeConsumed = false
    isCanBeResolved = true
    isTransitive = false
}

val maplibreJavaClasses = maplibreJavaApi.incoming.artifactView {
    attributes.attribute(ArtifactTypeDefinition.ARTIFACT_TYPE_ATTRIBUTE, "android-classes-jar")
}.files

group = pluginGroup.get()
version = pluginVersion.get()

android {
    namespace = "org.maplibre.plugins.shadows"
    compileSdk = 34
    ndkVersion = "28.2.13676358"

    defaultConfig {
        minSdk = 23
        if (pluginAbis != null) {
            ndk { abiFilters += pluginAbis.split(',').map(String::trim).filter { it.isNotEmpty() } }
        }
        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-fvisibility=hidden")
                arguments += listOf(
                    // Plugins may privately use C++ and a static STL. Only the
                    // pure-C plugin API crosses the DSO boundary.
                    "-DANDROID_STL=c++_static",
                    "-DMLN_SHADOW_PLUGIN_VERSION=${pluginVersion.get()}"
                )
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("android/src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets {
        getByName("main") {
            manifest.srcFile("android/src/main/AndroidManifest.xml")
            java.setSrcDirs(listOf("android/src/main/java"))
        }
    }

    buildFeatures { prefab = true }

    // The application-selected MapLibre renderer owns the host library. A
    // plugin may privately embed c++_static, but no C++ type crosses the ABI.
    packaging.jniLibs.excludes += setOf("**/libc++_shared.so", "**/libmaplibre.so")

    publishing {
        singleVariant("release") {
            withSourcesJar()
        }
    }
}

val generateVulkanShaders by tasks.registering(Exec::class) {
    group = "build"
    description = "Regenerates the checked-in Vulkan SPIR-V and C include files"
    val sdkRoot = providers.environmentVariable("ANDROID_HOME")
        .orElse(providers.environmentVariable("ANDROID_SDK_ROOT"))
    val ndkRoot = file("${sdkRoot.get()}/ndk/${android.ndkVersion}")
    commandLine(file("android/src/main/cpp/shaders/generate_shaders.sh"), ndkRoot)
}

dependencies {
    // The dedicated artifact supplies the canonical C ABI through Prefab
    // without introducing a renderer library into the plugin AAR.
    implementation("org.maplibre.gl:android-plugin-api:${maplibreVersion.get()}")
    maplibreJavaApi("org.maplibre.gl:android-sdk:${maplibreJavaApiVersion.get()}@aar")
    compileOnly(files(maplibreJavaClasses))
    compileOnly("androidx.annotation:annotation:1.8.2")
    testImplementation("junit:junit:4.13.2")
}

publishing {
    publications {
        register<MavenPublication>("release") {
            groupId = project.group.toString()
            artifactId = "fill-extrusion-shadows"
            version = project.version.toString()
            afterEvaluate { from(components["release"]) }
            pom {
                name.set("MapLibre Fill Extrusion Shadows")
                description.set("Runtime fill-extrusion shadow plugin for MapLibre Native")
                url.set("https://github.com/louwers/maplibre-native-plugin-template")
                licenses {
                    license {
                        name.set("BSD-2-Clause")
                        url.set("https://opensource.org/license/bsd-2-clause")
                    }
                }
            }
        }
    }
}
