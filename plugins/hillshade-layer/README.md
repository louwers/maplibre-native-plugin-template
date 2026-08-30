# Hillshade layer

This plugin implements MapLibre Native's built-in hillshade behavior as the
source-bound layer type `org.maplibre.hillshade`. It consumes a `raster-dem`
source and uses the plugin API's host-owned two-pass render graph on OpenGL,
Vulkan, and Metal.

Register the plugin before loading a style, then replace a built-in layer's
`"type": "hillshade"` with `"type": "org.maplibre.hillshade"`. The existing
`hillshade-*` paint properties are unchanged.
