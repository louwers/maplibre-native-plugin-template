package org.maplibre.plugins.shadows.demo;

import android.view.View;
import android.widget.Button;

import org.maplibre.android.camera.CameraPosition;
import org.maplibre.android.maps.Style;
import org.maplibre.android.style.layers.Layer;
import org.maplibre.plugins.shadows.FillExtrusionShadows;

public final class ShadowDemoActivity extends MapDemoActivity {
  private static final String BUILDING_LAYER = "building-3d";
  private boolean shadowEnabled = true;

  @Override protected int titleResource() { return R.string.shadows_title; }
  @Override protected String styleUri() { return "asset://liberty-shadow.json"; }
  @Override protected CameraPosition initialCamera() {
    return camera(52.5206, 13.4098, 16.6, 58.0, -22.0);
  }

  @Override
  protected void configureAction(Button button) {
    button.setVisibility(View.VISIBLE);
    button.setText(R.string.disable_shadows);
    button.setOnClickListener(view -> {
      shadowEnabled = !shadowEnabled;
      applyShadowProperty();
      button.setText(shadowEnabled ? R.string.disable_shadows : R.string.enable_shadows);
    });
  }

  @Override protected void onStyleLoaded(Style style) { applyShadowProperty(); }

  private void applyShadowProperty() {
    if (map == null || map.getStyle() == null) return;
    Layer layer = map.getStyle().getLayer(BUILDING_LAYER);
    if (layer != null) layer.setProperties(FillExtrusionShadows.fillExtrusionShadow(shadowEnabled));
  }

  void setShadowEnabledForTest(boolean enabled) {
    shadowEnabled = enabled;
    applyShadowProperty();
  }
}
