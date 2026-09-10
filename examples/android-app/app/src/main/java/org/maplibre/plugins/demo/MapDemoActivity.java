package org.maplibre.plugins.demo;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.maplibre.android.camera.CameraPosition;
import org.maplibre.android.geometry.LatLng;
import org.maplibre.android.maps.MapLibreMap;
import org.maplibre.android.maps.MapView;
import org.maplibre.android.maps.Style;

/** Shared map and lifecycle plumbing; each plugin remains a separate activity. */
abstract class MapDemoActivity extends Activity {
  protected MapLibreMap map;
  protected Button actionButton;
  private MapView mapView;

  protected abstract int titleResource();
  protected abstract String styleUri();
  protected abstract CameraPosition initialCamera();
  protected int layoutResource() { return R.layout.activity_map_demo; }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(layoutResource());

    mapView = findViewById(R.id.map_view);
    mapView.onCreate(savedInstanceState);
    TextView title = findViewById(R.id.demo_title);
    if (title != null) title.setText(titleResource());
    View back = findViewById(R.id.back_button);
    if (back != null) back.setOnClickListener(view -> finish());
    actionButton = findViewById(R.id.demo_action);
    if (actionButton != null) configureAction(actionButton);

    mapView.getMapAsync(readyMap -> {
      map = readyMap;
      map.setCameraPosition(initialCamera());
      loadStyle();
    });
  }

  protected void loadStyle() {
    map.setStyle(new Style.Builder().fromUri(styleUri()), this::onStyleLoaded);
  }

  protected void configureAction(Button button) {
    button.setVisibility(View.GONE);
  }

  protected void onStyleLoaded(Style style) {
  }

  protected static CameraPosition camera(double latitude,
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
