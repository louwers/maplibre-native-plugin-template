package org.maplibre.plugins.shadows.demo;

import android.app.Activity;
import android.os.Bundle;

import org.maplibre.android.camera.CameraPosition;
import org.maplibre.android.geometry.LatLng;
import org.maplibre.android.maps.MapView;
import org.maplibre.android.maps.Style;

public final class MainActivity extends Activity {
  private MapView mapView;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    mapView = findViewById(R.id.map_view);
    mapView.onCreate(savedInstanceState);

    findViewById(R.id.shadow_toggle).setVisibility(android.view.View.GONE);
    findViewById(R.id.scene_toggle).setVisibility(android.view.View.GONE);

    mapView.getMapAsync(readyMap -> {
      readyMap.setCameraPosition(new CameraPosition.Builder()
          .target(new LatLng(48.8566, 2.3522))
          .zoom(12.0)
          .tilt(0.0)
          .bearing(0.0)
          .build());
      readyMap.setStyle(new Style.Builder().fromUri("asset://rectangle-style.json"));
    });
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
