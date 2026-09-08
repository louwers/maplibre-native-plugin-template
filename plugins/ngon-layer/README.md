# N-gon layer

Regular convex polygons centered on GeoJSON or vector-tile Point/MultiPoint features.
Register `NgonLayerPlugin` (Android) or `NgonLayer` (iOS) before loading the style.
The style layer type is `ngon`; the plugin ID is `org.maplibre.ngon-layer`.

```json
{
  "id": "markers",
  "type": "ngon",
  "source": "points",
  "paint": {
    "ngon-corners": ["get", "corners"],
    "ngon-radius": ["interpolate", ["linear"], ["zoom"], 10, 6, 16, 24],
    "ngon-rotate": ["get", "heading"],
    "ngon-color": ["get", "color"],
    "ngon-stroke-width": 2,
    "ngon-stroke-color": "#ffffff"
  }
}
```

Every property below supports constants, camera, feature, composite and feature-state
expressions. Numeric values and colors support transitions; enum strings do not.
All properties are paint properties. Ordinary source-layer, filter, visibility and
minzoom/maxzoom behavior comes from the host's source-bound layer implementation.

| Property | Default | Meaning |
| --- | --- | --- |
| `ngon-radius` | `5` | Circumradius in logical pixels; nonnegative. |
| `ngon-corners` | `5` | 3–360 corners, rounded to the nearest integer. |
| `ngon-rotate` | `0` | Clockwise degrees; zero places one vertex upward. |
| `ngon-color` | `#000000` | Fill color, including alpha. |
| `ngon-opacity` | `1` | Fill opacity, 0–1. |
| `ngon-blur` | `0` | Inward edge feather as a fraction of radius, 0–1. |
| `ngon-stroke-width` | `0` | Outside stroke width in logical pixels; mitered corners. |
| `ngon-stroke-color` | `#000000` | Stroke color, including alpha. |
| `ngon-stroke-opacity` | `1` | Stroke opacity, 0–1. |
| `ngon-translate` | `[0, 0]` | Pixel offset: right, down. |
| `ngon-translate-anchor` | `map` | `map` rotates the offset with the map; `viewport` does not. |
| `ngon-pitch-alignment` | `viewport` | `viewport` faces the camera; `map` lies in the map plane. |
| `ngon-pitch-scale` | `map` | `map` scales with perspective; `viewport` maintains size. |

Rotation is measured in the alignment plane. The map-aligned case rotates with the
map. As with circle markers, this is not a collision-placed symbol layer. Sort keys,
terrain/globe projection, patterns and collision placement are not implemented.
Transparent paint is still queryable, matching geometric hit testing; zero radius
and zero stroke produce no hit. Queries use the actual rotated polygon, not its quad.

## Implementation and validation

A four-vertex host-owned drawable carries each point. The shaders evaluate the
polygon analytically, with derivative antialiasing and premultiplied color blending.
Half-open tile ownership avoids duplicate buffered markers; marker quads are not
stencil-clipped at tile seams. Scalar and enum zoom endpoints are packed into float2
attributes so all thirteen properties can be feature-driven at once within the
portable sixteen-attribute limit. Geometry and GPU resources stay owned by MapLibre.

The same generator produces OpenGL ES, Vulkan and Metal shader sources:

```sh
node plugins/ngon-layer/scripts/generate-shaders.mjs --check
cmake --build build-Metal --target ngon-unit-tests
ctest --test-dir build-Metal -R '^ngon-unit-tests$' --output-on-failure
```

The shared render runner discovers this plugin's manifest automatically. Its cases
cover corner counts/rotation, every property feature-driven simultaneously, fractional
composite zoom, feature state, runtime paint updates, tile seams/MultiPoint, blur,
translation and all four pitch alignment/scale combinations. Expected images are
shared across backends; backend-specific skips or baselines are not needed.
