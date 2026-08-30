package org.maplibre.plugins.shadows.demo;

import org.maplibre.android.camera.CameraPosition;

public final class GltfDemoActivity extends MapDemoActivity {
  @Override protected int titleResource() { return R.string.gltf_title; }
  @Override protected String styleUri() { return "asset://positron-gltf.json"; }
  @Override protected CameraPosition initialCamera() {
    return camera(48.8582621, 2.2944962, 15.7, 62.0, -28.0);
  }
}
