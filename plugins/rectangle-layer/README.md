# Rectangle layer

The rectangle plugin registers a source-bound `rectangle` style layer. It consumes Point and MultiPoint features from GeoJSON or vector-tile geometry sources. Every point becomes a centered, viewport-aligned rectangle measured in logical screen pixels.

```json
{
  "id": "points",
  "type": "rectangle",
  "source": "points",
  "paint": {
    "rectangle-color": "#e00",
    "rectangle-width": 12,
    "rectangle-height": 12,
    "rectangle-stroke-width": 1,
    "rectangle-stroke-color": "#000"
  }
}
```

All five paint properties accept MapLibre expressions. The host evaluates them per feature during tile layout, copies the returned CPU bucket, compiles the plugin-provided OpenGL/Vulkan/Metal shader, and owns every resulting GPU buffer and drawable. Register the plugin before loading a style containing the layer.

Android:

```java
RectangleLayerPlugin.register();
```

iOS:

```swift
try RectangleLayerPlugin.registerPlugin()
```
