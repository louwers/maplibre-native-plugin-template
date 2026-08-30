package org.maplibre.plugins.shadows.demo;

import org.maplibre.android.camera.CameraPosition;

public final class RectangleDemoActivity extends MapDemoActivity {
  @Override protected int titleResource() { return R.string.rectangles_title; }
  @Override protected String styleUri() { return "asset://rectangle-style.json"; }
  @Override protected CameraPosition initialCamera() {
    return camera(48.8566, 2.3522, 12.0, 0.0, 0.0);
  }
}
