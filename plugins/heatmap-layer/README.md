# Heatmap layer plugin

This plugin registers the source-bound style layer type `org.maplibre.heatmap`.
It implements the built-in MapLibre heatmap behavior through the C plugin API:
point layout, data-driven radius and weight, a half-resolution floating-point
density target, additive Gaussian kernels, and color-ramp compositing.

The built-in `heatmap` layer remains available. Register this plugin before
loading a style that contains `"type": "org.maplibre.heatmap"`.
