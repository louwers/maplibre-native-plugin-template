package org.maplibre.plugins.shadows.demo;

import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.SystemClock;

import androidx.test.core.app.ActivityScenario;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.maplibre.android.plugins.MapLibrePluginRegistry;
import org.maplibre.plugins.shadows.FillExtrusionShadowsPlugin;

@RunWith(AndroidJUnit4.class)
public final class PluginSmokeTest {
  @Test
  public void pluginRegistersRendersTogglesAndRotates() {
    FillExtrusionShadowsPlugin.register();
    assertTrue(MapLibrePluginRegistry.isRegistered(FillExtrusionShadowsPlugin.ID));

    Intent intent = new Intent(ApplicationProvider.getApplicationContext(), MainActivity.class);
    intent.putExtra("scene", "shadows");
    try (ActivityScenario<MainActivity> scenario = ActivityScenario.launch(intent)) {
      waitForCallbackCountGreaterThan(0, 30_000);

      long beforeRotation = FillExtrusionShadowsPlugin.renderCallbackCount();
      scenario.onActivity(activity -> activity.setRequestedOrientation(
          ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE));
      waitForCallbackCountGreaterThan(beforeRotation, 10_000);

      scenario.onActivity(activity -> activity.setShadowEnabledForTest(false));
      SystemClock.sleep(1_500);
      long disabledCount = FillExtrusionShadowsPlugin.renderCallbackCount();
      SystemClock.sleep(1_000);
      assertTrue("disabled shadows must stop composites",
          FillExtrusionShadowsPlugin.renderCallbackCount() <= disabledCount + 1);

      scenario.onActivity(activity -> activity.setShadowEnabledForTest(true));
      waitForCallbackCountGreaterThan(disabledCount + 1, 10_000);
    }
  }

  private static void waitForCallbackCountGreaterThan(long count, long timeoutMillis) {
    long deadline = SystemClock.uptimeMillis() + timeoutMillis;
    while (SystemClock.uptimeMillis() < deadline) {
      if (FillExtrusionShadowsPlugin.renderCallbackCount() > count) return;
      SystemClock.sleep(250);
    }
    assertTrue("plugin render callback did not execute", false);
  }
}
