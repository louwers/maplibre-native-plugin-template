package org.maplibre.plugins.shadows.demo;

import android.view.View;
import android.widget.Button;

import org.maplibre.android.camera.CameraPosition;
import org.maplibre.android.camera.CameraUpdateFactory;

public final class RectangleDemoActivity extends MapDemoActivity {
  private boolean zoomedIn;

  @Override protected int titleResource() { return R.string.rectangles_title; }
  @Override protected String styleUri() { return "asset://rectangle-style.json"; }
  @Override protected CameraPosition initialCamera() {
    return camera(48.8566, 2.3522, 10.0, 0.0, 0.0);
  }

  @Override
  protected void configureAction(Button button) {
    button.setVisibility(View.VISIBLE);
    button.setText(R.string.animate_zoom_in);
    button.setOnClickListener(view -> {
      if (map == null) return;
      final double targetZoom = zoomedIn ? 10.0 : 16.0;
      map.animateCamera(CameraUpdateFactory.zoomTo(targetZoom), 10_000);
      zoomedIn = !zoomedIn;
      button.setText(zoomedIn ? R.string.animate_zoom_out : R.string.animate_zoom_in);
    });
  }
}
