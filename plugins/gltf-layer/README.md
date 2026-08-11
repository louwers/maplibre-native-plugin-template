# GLTF layer

Registers a source-less `gltf` style layer backed by TinyGLTF. Model downloads use MapLibre Native's file loader,
including its network policy, resource transformation, and cache.

```json
{
  "id": "eiffel-tower",
  "type": "gltf",
  "minzoom": 14,
  "layout": {
    "model-uri": "https://3dmr.eu/api/model/4/3",
    "model-position": [2.2945, 48.8584],
    "model-altitude": 0,
    "model-heading": 0,
    "model-scale": 3.2
  },
  "paint": { "model-opacity": 1 }
}
```

The initial implementation supports static triangle meshes, node transforms, material base colors, alpha blending,
and double-sided geometry in GLB files. Animation, skinning, morph targets, mesh compression, and terrain anchoring are
deferred.
