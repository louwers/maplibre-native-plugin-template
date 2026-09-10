package org.maplibre.plugins.demo;

import android.util.Log;

import org.maplibre.android.camera.CameraPosition;
import org.maplibre.android.maps.Style;

import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Map-only demonstration using real OpenFreeMap capital features. */
public final class NgonDemoActivity extends MapDemoActivity {
  private final ExecutorService loader = Executors.newSingleThreadExecutor();

  @Override protected int titleResource() { return R.string.ngon_title; }
  @Override protected int layoutResource() { return R.layout.activity_capital_map; }
  @Override protected String styleUri() { return CapitalStyle.STYLE_URL; }
  @Override protected CameraPosition initialCamera() { return camera(50.0, 10.0, 4.15, 0, 0); }

  @Override protected void loadStyle() {
    loader.execute(() -> {
      HttpURLConnection connection = null;
      try {
        connection = (HttpURLConnection) new URL(styleUri()).openConnection();
        connection.setConnectTimeout(15_000);
        connection.setReadTimeout(15_000);
        if (connection.getResponseCode() != 200) throw new java.io.IOException("Style HTTP " + connection.getResponseCode());
        final String json;
        try (InputStream base = connection.getInputStream();
             InputStream layers = getAssets().open("ngon-capitals.layers.json")) {
          json = CapitalStyle.compose(read(base), read(layers));
        }
        runOnUiThread(() -> {
          if (!isFinishing() && !isDestroyed()) map.setStyle(new Style.Builder().fromJson(json), this::onStyleLoaded);
        });
      } catch (Exception error) {
        Log.e("NgonCapitals", "Unable to load OpenFreeMap", error);
      } finally {
        if (connection != null) connection.disconnect();
      }
    });
  }

  static String read(InputStream input) throws java.io.IOException {
    java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
    byte[] buffer = new byte[8192];
    for (int count; (count = input.read(buffer)) != -1;) bytes.write(buffer, 0, count);
    return bytes.toString(StandardCharsets.UTF_8.name());
  }

  @Override protected void onStyleLoaded(Style style) {
    Log.i("NgonCapitals", "OpenFreeMap capital style loaded");
  }

  @Override protected void onDestroy() {
    loader.shutdownNow();
    super.onDestroy();
  }
}
