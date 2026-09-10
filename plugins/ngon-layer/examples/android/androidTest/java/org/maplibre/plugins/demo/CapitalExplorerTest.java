package org.maplibre.plugins.demo;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.RectF;
import android.os.SystemClock;
import org.maplibre.android.style.expressions.Expression;
import org.maplibre.plugins.ngon.Ngons;

import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.maplibre.geojson.Feature;

import java.io.InputStream;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Predicate;

import static org.junit.Assert.*;

@RunWith(AndroidJUnit4.class)
public final class CapitalExplorerTest {
  @Test public void composesRealVectorSourceAndKeepsAttribution() throws Exception {
    String overlays;
    try (InputStream input = InstrumentationRegistry.getInstrumentation().getTargetContext()
        .getAssets().open("ngon-capitals.layers.json")) {
      overlays = NgonDemoActivity.read(input);
    }
    JSONObject style = new JSONObject(CapitalStyle.compose(
        "{\"version\":8,\"sources\":{\"renamed-provider\":{\"type\":\"vector\",\"url\":\"https://tiles.openfreemap.org/planet\",\"attribution\":\"OpenFreeMap\"}},"
        + "\"layers\":[{\"id\":\"cities\",\"type\":\"symbol\",\"source\":\"renamed-provider\",\"source-layer\":\"place\"}]}", overlays));
    JSONArray layers = style.getJSONArray("layers");
    assertEquals(3, layers.length());
    assertEquals("renamed-provider", layers.getJSONObject(1).getString("source"));
    assertEquals("place", layers.getJSONObject(1).getString("source-layer"));
    assertEquals("ngon", layers.getJSONObject(1).getString("type"));
    assertEquals(new JSONArray(CapitalStyle.FILTER).toString(), layers.getJSONObject(1).getJSONArray("filter").toString());
    assertEquals("OpenFreeMap", style.getJSONObject("sources").getJSONObject("renamed-provider").getString("attribution"));
    assertTrue(layers.getJSONObject(0).getJSONArray("filter").toString().contains("!"));
    assertEquals(4, CapitalStyle.level(Feature.fromJson("{\"type\":\"Feature\",\"geometry\":null,\"properties\":{\"capital\":\"4\"}}")));
    assertEquals(0, CapitalStyle.level(Feature.fromJson("{\"type\":\"Feature\",\"geometry\":null,\"properties\":{}}")));
  }

  /** Network/device test: real provider tiles must yield both shapes and rendered pixels. */
  @Test public void rendersLiveCapitalsAndRuntimeFilter() throws Exception {
    try (ActivityScenario<NgonDemoActivity> scenario = ActivityScenario.launch(NgonDemoActivity.class)) {
      await(scenario, activity -> activity.map != null && activity.map.getStyle() != null
          && activity.map.getStyle().getLayer(CapitalStyle.MARKERS) != null);
      scenario.onActivity(activity -> activity.map.setCameraPosition(MapDemoActivity.camera(51.0, 10.0, 5.7, 0, 0)));
      await(scenario, activity -> {
        List<Feature> features = visible(activity);
        return activity.map.getCameraPosition().zoom > 5.6
            && features.stream().anyMatch(feature -> CapitalStyle.level(feature) == 2)
            && features.stream().anyMatch(feature -> CapitalStyle.level(feature) == 4);
      });
      // Allow the camera transition and its final repaint to finish.
      SystemClock.sleep(1000);
      Bitmap all = snapshot(scenario);
      assertTrue("National gold n-gon pixels", pixels(all, 229, 172, 66) > 50);
      assertTrue("Regional teal n-gon pixels", pixels(all, 33, 139, 145) > 50);
      scenario.onActivity(activity -> activity.map.getStyle().getLayer(CapitalStyle.MARKERS).setProperties(
          Ngons.opacity(Expression.raw("[\"case\",[\"==\",[\"to-number\",[\"get\",\"capital\"],0],2],0.95,0]"))));
      SystemClock.sleep(700);
      Bitmap national = snapshot(scenario);
      assertTrue("National markers remain", pixels(national, 229, 172, 66) > 50);
      assertTrue("Regional markers disappear", pixels(national, 33, 139, 145) < pixels(all, 33, 139, 145) / 5);
      scenario.onActivity(activity -> activity.map.getStyle().getLayer(CapitalStyle.MARKERS).setProperties(Ngons.opacity(0.95f)));
      SystemClock.sleep(700);
      assertTrue("Regional markers return", pixels(snapshot(scenario), 33, 139, 145) > 50);
    }
  }

  private static List<Feature> visible(NgonDemoActivity activity) {
    android.view.View view = activity.findViewById(R.id.map_view);
    return activity.map.queryRenderedFeatures(new RectF(0, 0, view.getWidth(), view.getHeight()), CapitalStyle.MARKERS);
  }

  private static void await(ActivityScenario<NgonDemoActivity> scenario, Predicate<NgonDemoActivity> ready) {
    long deadline = SystemClock.elapsedRealtime() + 90_000;
    AtomicBoolean result = new AtomicBoolean();
    while (SystemClock.elapsedRealtime() < deadline) {
      scenario.onActivity(activity -> result.set(ready.test(activity)));
      if (result.get()) return;
      SystemClock.sleep(250);
    }
    fail("Timed out waiting for the OpenFreeMap capital scene");
  }

  private static Bitmap snapshot(ActivityScenario<NgonDemoActivity> scenario) throws Exception {
    CountDownLatch latch = new CountDownLatch(1);
    AtomicReference<Bitmap> bitmap = new AtomicReference<>();
    scenario.onActivity(activity -> activity.map.snapshot(image -> { bitmap.set(image); latch.countDown(); }));
    assertTrue("Map snapshot", latch.await(15, TimeUnit.SECONDS));
    assertNotNull(bitmap.get());
    return bitmap.get();
  }

  private static int pixels(Bitmap image, int red, int green, int blue) {
    int count = 0;
    for (int y = 0; y < image.getHeight(); y += 2) {
      for (int x = 0; x < image.getWidth(); x += 2) {
        int pixel = image.getPixel(x, y);
        if (Math.abs(Color.red(pixel) - red) < 20 && Math.abs(Color.green(pixel) - green) < 20
            && Math.abs(Color.blue(pixel) - blue) < 20) count++;
      }
    }
    return count;
  }
}
