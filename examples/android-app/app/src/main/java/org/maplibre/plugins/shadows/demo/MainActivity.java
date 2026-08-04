package org.maplibre.plugins.shadows.demo;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Button;

import org.maplibre.android.camera.CameraPosition;
import org.maplibre.android.geometry.LatLng;
import org.maplibre.android.maps.MapLibreMap;
import org.maplibre.android.maps.MapView;
import org.maplibre.android.maps.Style;
import org.maplibre.android.style.layers.Layer;
import org.maplibre.plugins.shadows.FillExtrusionShadows;

public final class MainActivity extends Activity {
  private static final String BUILDING_LAYER = "building-3d";

  private MapView mapView;
  private MapLibreMap map;
  private boolean shadowEnabled = true;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    mapView = findViewById(R.id.map_view);
    mapView.onCreate(savedInstanceState);

    Button toggle = findViewById(R.id.shadow_toggle);
    toggle.setOnClickListener(view -> {
      shadowEnabled = !shadowEnabled;
      applyShadowProperty();
      toggle.setText(shadowEnabled ? R.string.disable_shadows : R.string.enable_shadows);
    });

    mapView.getMapAsync(readyMap -> {
      map = readyMap;
      map.setCameraPosition(new CameraPosition.Builder()
          .target(new LatLng(52.5206, 13.4098))
          .zoom(16.6)
          .tilt(58.0)
          .bearing(-22.0)
          .build());
      map.setStyle(new Style.Builder().fromUri("asset://liberty-shadow.json"), style ->
          applyShadowProperty());
    });
  }

  private void applyShadowProperty() {
    if (map == null || map.getStyle() == null) return;
    Layer layer = map.getStyle().getLayer(BUILDING_LAYER);
    if (layer != null) {
      layer.setProperties(FillExtrusionShadows.fillExtrusionShadow(shadowEnabled));
    }
  }

  void setShadowEnabledForTest(boolean enabled) {
    shadowEnabled = enabled;
    applyShadowProperty();
  }

  @Override protected void onStart() { super.onStart(); mapView.onStart(); }
  @Override protected void onResume() { super.onResume(); mapView.onResume(); }
  @Override protected void onPause() { mapView.onPause(); super.onPause(); }
  @Override protected void onStop() { mapView.onStop(); super.onStop(); }
  @Override public void onLowMemory() { super.onLowMemory(); mapView.onLowMemory(); }
  @Override protected void onDestroy() { mapView.onDestroy(); super.onDestroy(); }

  @Override
  protected void onSaveInstanceState(Bundle outState) {
    super.onSaveInstanceState(outState);
    mapView.onSaveInstanceState(outState);
  }
}
