package org.maplibre.plugins.shadows.demo;

import android.app.Application;

import org.maplibre.android.MapLibre;
import org.maplibre.plugins.rectangle.RectangleLayerPlugin;

public final class DemoApplication extends Application {
  @Override
  public void onCreate() {
    super.onCreate();
    MapLibre.getInstance(this);
    RectangleLayerPlugin.register();
  }
}
