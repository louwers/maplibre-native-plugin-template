plugins {
    id("com.android.application")
}

val maplibreVersion = providers.gradleProperty("maplibreVersion")

android {
    namespace = "org.maplibre.plugins.shadows.demo"
    compileSdk = 34

    defaultConfig {
        applicationId = "org.maplibre.plugins.shadows.demo"
        minSdk = 23
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    flavorDimensions += "renderer"
    productFlavors {
        create("opengl") { dimension = "renderer" }
        create("vulkan") { dimension = "renderer" }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

dependencies {
    "openglImplementation"("org.maplibre.gl:android-sdk-opengl:${maplibreVersion.get()}")
    "vulkanImplementation"("org.maplibre.gl:android-sdk-vulkan:${maplibreVersion.get()}")
    implementation(project(":plugins:fill-extrusion-shadows"))
    implementation(project(":plugins:gltf-layer"))
    implementation(project(":plugins:heatmap-layer"))
    implementation(project(":plugins:rectangle-layer"))
    androidTestImplementation("androidx.test:core:1.7.0")
    androidTestImplementation("androidx.test:runner:1.7.0")
    androidTestImplementation("androidx.test:rules:1.7.0")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
}
