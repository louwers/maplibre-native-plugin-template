package org.maplibre.plugins.demo;

import android.app.Application;

import org.maplibre.android.MapLibre;
import org.maplibre.plugins.ngon.NgonLayerPlugin;
import org.maplibre.plugins.gltf.GltfLayerPlugin;
import org.maplibre.plugins.heatmap.HeatmapLayerPlugin;
import org.maplibre.plugins.rectangle.RectangleLayerPlugin;

public final class DemoApplication extends Application {
  @Override
  public void onCreate() {
    super.onCreate();
    MapLibre.getInstance(this);
    NgonLayerPlugin.register();
    GltfLayerPlugin.register();
    HeatmapLayerPlugin.register();
    RectangleLayerPlugin.register();
  }
}
