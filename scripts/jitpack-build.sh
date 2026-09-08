#!/usr/bin/env bash
set -euo pipefail

# JitPack supplies VERSION and GROUP. Build every plugin with the same pinned
# C header, without making any renderer AAR a transitive runtime dependency.
: "${VERSION:?JitPack VERSION is required}"
: "${GROUP:?JitPack GROUP is required}"
plugin_root="$(cd "$(dirname "$0")/.." && pwd)"
native_revision="$(tr -d '\n' < "$plugin_root/native-revision.txt")"
[[ "$native_revision" =~ ^[0-9a-f]{40}$ ]] || { echo 'Invalid native revision' >&2; exit 1; }
native_checkout="$(mktemp -d)"
git -C "$native_checkout" init --quiet
git -C "$native_checkout" remote add origin https://github.com/maplibre/maplibre-native.git
git -C "$native_checkout" fetch --depth=1 origin "$native_revision"
git -C "$native_checkout" checkout --detach FETCH_HEAD
diff -u "$native_checkout/include/mln/plugin/plugin_api.h" \
    "$plugin_root/MapLibrePluginApi/include/mln/plugin/plugin_api.h"
api_version="0.0.0-plugin-${native_revision:0:12}-SNAPSHOT"
"$native_checkout/platform/android/gradlew" -p "$native_checkout/platform/android" \
    :android-plugin-api:publishReleasePublicationToMavenLocal -PmaplibreVersion="$api_version"
cd "$plugin_root"
MAPLIBRE_REPOSITORY_URL="file://${HOME}/.m2/repository" ./gradlew publishToMavenLocal \
    -PmaplibreVersion="$api_version" -PpluginVersion="$VERSION" -PpluginGroup="$GROUP"
