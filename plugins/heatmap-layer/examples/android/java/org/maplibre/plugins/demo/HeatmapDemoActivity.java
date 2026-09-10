package org.maplibre.plugins.demo;

import org.maplibre.android.camera.CameraPosition;

public final class HeatmapDemoActivity extends MapDemoActivity {
  @Override protected int titleResource() { return R.string.heatmap_title; }
  @Override protected String styleUri() { return "asset://heatmap-style.json"; }
  @Override protected CameraPosition initialCamera() {
    return camera(48.8566, 2.3522, 11.2, 0.0, 0.0);
  }
}
