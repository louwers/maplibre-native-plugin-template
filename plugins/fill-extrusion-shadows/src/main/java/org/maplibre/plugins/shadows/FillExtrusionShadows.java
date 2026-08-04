package org.maplibre.plugins.shadows;

import org.maplibre.android.style.layers.PaintPropertyValue;

/** Style-property helpers for the fill-extrusion shadows plugin. */
public final class FillExtrusionShadows {
  public static final String PROPERTY_FILL_EXTRUSION_SHADOW = "fill-extrusion-shadow";

  private FillExtrusionShadows() {
  }

  public static PaintPropertyValue<Boolean> fillExtrusionShadow(boolean enabled) {
    return new PaintPropertyValue<>(PROPERTY_FILL_EXTRUSION_SHADOW, enabled);
  }
}

