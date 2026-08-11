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
  private Button shadowToggle;
  private Button sceneToggle;
  private boolean shadowEnabled = true;
  private boolean showingModel = true;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    mapView = findViewById(R.id.map_view);
    mapView.onCreate(savedInstanceState);

    shadowToggle = findViewById(R.id.shadow_toggle);
    shadowToggle.setEnabled(false);
    shadowToggle.setOnClickListener(view -> {
      shadowEnabled = !shadowEnabled;
      applyShadowProperty();
      shadowToggle.setText(shadowEnabled ? R.string.disable_shadows : R.string.enable_shadows);
    });
    sceneToggle = findViewById(R.id.scene_toggle);
    sceneToggle.setText(R.string.show_shadows);
    sceneToggle.setOnClickListener(view -> {
      showingModel = !showingModel;
      showScene();
    });

    mapView.getMapAsync(readyMap -> {
      map = readyMap;
      map.setCameraPosition(new CameraPosition.Builder()
          .target(new LatLng(48.8582621, 2.2944962))
          .zoom(15.7)
          .tilt(62.0)
          .bearing(-28.0)
          .build());
      map.setStyle(new Style.Builder().fromUri("asset://positron-gltf.json"), style -> showCamera(false));
    });
  }

  private void showScene() {
    if (map == null) return;
    shadowToggle.setEnabled(!showingModel);
    sceneToggle.setText(showingModel ? R.string.show_shadows : R.string.show_gltf);
    String styleUri = showingModel ? "asset://positron-gltf.json" : "asset://liberty-shadow.json";
    map.setStyle(new Style.Builder().fromUri(styleUri), style -> {
      if (!showingModel) applyShadowProperty();
    });
    showCamera(true);
  }

  private void showCamera(boolean animated) {
    CameraPosition camera =
        new CameraPosition.Builder()
            .target(showingModel ? new LatLng(48.8582621, 2.2944962) : new LatLng(52.5206, 13.4098))
            .zoom(showingModel ? 15.7 : 16.6)
            .tilt(showingModel ? 62.0 : 58.0)
            .bearing(showingModel ? -28.0 : -22.0)
            .build();
    if (animated) {
      map.animateCamera(org.maplibre.android.camera.CameraUpdateFactory.newCameraPosition(camera), 1200);
    } else {
      map.setCameraPosition(camera);
    }
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
