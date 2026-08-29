package org.maplibre.plugins.shadows.demo;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Button;

import org.maplibre.android.camera.CameraPosition;
import org.maplibre.android.geometry.LatLng;
import org.maplibre.android.maps.MapView;
import org.maplibre.android.maps.MapLibreMap;
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
  private int scene = 1;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    scene = sceneFromName(getIntent().getStringExtra("scene"));
    setContentView(R.layout.activity_main);
    mapView = findViewById(R.id.map_view);
    mapView.onCreate(savedInstanceState);

    shadowToggle = findViewById(R.id.shadow_toggle);
    shadowToggle.setOnClickListener(view -> {
      shadowEnabled = !shadowEnabled;
      applyShadowProperty();
      shadowToggle.setText(shadowEnabled ? R.string.disable_shadows : R.string.enable_shadows);
    });
    sceneToggle = findViewById(R.id.scene_toggle);
    sceneToggle.setOnClickListener(view -> {
      scene = (scene + 1) % 3;
      showScene();
    });

    mapView.getMapAsync(readyMap -> {
      map = readyMap;
      showScene();
    });
  }

  private static int sceneFromName(String name) {
    if ("shadows".equals(name)) return 0;
    if ("rectangle".equals(name)) return 2;
    return 1;
  }

  private void showScene() {
    if (map == null) return;
    final String style;
    final CameraPosition camera;
    if (scene == 0) {
      style = "asset://liberty-shadow.json";
      camera = camera(52.5206, 13.4098, 16.6, 58.0, -22.0);
      sceneToggle.setText(R.string.show_gltf);
    } else if (scene == 1) {
      style = "asset://positron-gltf.json";
      camera = camera(48.8582621, 2.2944962, 15.7, 62.0, -28.0);
      sceneToggle.setText(R.string.show_rectangles);
    } else {
      style = "asset://rectangle-style.json";
      camera = camera(48.8566, 2.3522, 12.0, 0.0, 0.0);
      sceneToggle.setText(R.string.show_shadows);
    }
    shadowToggle.setVisibility(scene == 0 ? android.view.View.VISIBLE : android.view.View.GONE);
    map.setCameraPosition(camera);
    map.setStyle(new Style.Builder().fromUri(style), loaded -> {
      if (scene == 0) applyShadowProperty();
    });
  }

  private static CameraPosition camera(double latitude,
                                       double longitude,
                                       double zoom,
                                       double tilt,
                                       double bearing) {
    return new CameraPosition.Builder()
        .target(new LatLng(latitude, longitude))
        .zoom(zoom)
        .tilt(tilt)
        .bearing(bearing)
        .build();
  }

  private void applyShadowProperty() {
    if (map == null || map.getStyle() == null) return;
    Layer layer = map.getStyle().getLayer(BUILDING_LAYER);
    if (layer != null) layer.setProperties(FillExtrusionShadows.fillExtrusionShadow(shadowEnabled));
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
