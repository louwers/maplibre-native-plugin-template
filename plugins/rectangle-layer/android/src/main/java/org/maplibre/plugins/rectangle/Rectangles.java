package org.maplibre.plugins.rectangle;

import org.maplibre.android.style.layers.PaintPropertyValue;

/** Paint-property helpers shared by all Android MapLibre layer wrappers. */
public final class Rectangles {
  private Rectangles() {}

  public static PaintPropertyValue<String> color(String color) {
    return new PaintPropertyValue<>("rectangle-color", color);
  }

  public static PaintPropertyValue<Float> width(float width) {
    return new PaintPropertyValue<>("rectangle-width", width);
  }

  public static PaintPropertyValue<Float> height(float height) {
    return new PaintPropertyValue<>("rectangle-height", height);
  }

  public static PaintPropertyValue<Float> strokeWidth(float width) {
    return new PaintPropertyValue<>("rectangle-stroke-width", width);
  }

  public static PaintPropertyValue<String> strokeColor(String color) {
    return new PaintPropertyValue<>("rectangle-stroke-color", color);
  }
}
