# MapLibre Native plugin template

This is an independent repository for native plugins that extend existing MapLibre style layers at runtime. It does not include or patch MapLibre Native source code.

The first example, `plugins/fill-extrusion-shadows`, registers the constant paint property `fill-extrusion-shadow` for `fill-extrusion` layers and renders projected shadows using geometry borrowed from the host for the duration of each callback.

## Build

MapLibre Android and its `android-plugin-api` companion must first be published at the same exact version. Point this build at that repository and snapshot:

```shell
export REPOSILITE_URL=https://reposilite.louwers.dev/snapshots
export REPOSILITE_USERNAME=...
export REPOSILITE_PASSWORD=...
./gradlew :plugins:fill-extrusion-shadows:assembleRelease \
  -PmaplibreVersion=13.4.1-plugin.example-SNAPSHOT
```

For local use, credentials may instead live outside this repository in `~/.gradle/gradle.properties`:

```properties
reposiliteUsername=...
reposilitePassword=...
```

Publish with an exact host and plugin snapshot pair:

```shell
./gradlew :plugins:fill-extrusion-shadows:publishReleasePublicationToReposiliteRepository \
  -PmaplibrePluginRepository=https://reposilite.louwers.dev/snapshots \
  -PmaplibreVersion=<exact-maplibre-snapshot> \
  -PpluginVersion=<exact-plugin-snapshot>
```

The app under `examples/android-app` has OpenGL and Vulkan product flavors and consumes only external Maven coordinates. It does not use project dependencies, a composite build, `mavenLocal`, or local AAR fallbacks.

The STL-free `android-plugin-api` dependency and the renderer Java API are `compileOnly` and are removed from the published POM. MapLibre and plugins may each use a private `c++_static` runtime; only C structs, callbacks, function pointers, and opaque handles cross the boundary. Applications select exactly one renderer artifact themselves.

See [designs/plugin-interface.md](designs/plugin-interface.md) for the API contract and lifecycle.
