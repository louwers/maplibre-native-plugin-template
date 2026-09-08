package org.maplibre.plugins.demo;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/** Launcher menu for the independent plugin demos. */
public final class MainActivity extends Activity {
  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);

    findViewById(R.id.demo_ngon).setOnClickListener(view -> open(NgonDemoActivity.class));
    findViewById(R.id.demo_gltf).setOnClickListener(view -> open(GltfDemoActivity.class));
    findViewById(R.id.demo_rectangles).setOnClickListener(view -> open(RectangleDemoActivity.class));
    findViewById(R.id.demo_heatmap).setOnClickListener(view -> open(HeatmapDemoActivity.class));
  }

  private void open(Class<? extends Activity> activity) {
    startActivity(new Intent(this, activity));
  }
}
