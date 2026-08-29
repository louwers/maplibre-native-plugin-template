# GLTF layer

Registers a source-bound `gltf` style layer backed by TinyGLTF. Point features provide the geographic anchors. Model
downloads run during CPU layout through MapLibre Native's file loader, including its network policy, resource
transformation, and cache. The plugin emits backend-neutral buckets and MapLibre owns every GPU drawable.

```json
{
  "id": "eiffel-tower",
  "type": "gltf",
  "source": "eiffel-tower-anchor",
  "minzoom": 14,
  "layout": {
    "model-uri": "https://3dmr.eu/api/model/4/3",
    "model-altitude": 0,
    "model-heading": 0,
    "model-scale": 3.2
  },
  "paint": { "model-opacity": 1 }
}
```

Here `eiffel-tower-anchor` is a GeoJSON source containing a point at `[2.2944962, 48.8582621]`. One model instance is
laid out for every point feature, so normal source filtering, tiling, visibility, zoom ranges, and layer ordering apply.

The initial implementation supports static indexed triangle meshes, node transforms, material base colors, alpha
blending, and GLB files. Animation, skinning, morph targets, textures, mesh compression, and terrain anchoring are
deferred.
