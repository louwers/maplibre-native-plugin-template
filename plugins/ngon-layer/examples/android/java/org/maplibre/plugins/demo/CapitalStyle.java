package org.maplibre.plugins.demo;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.maplibre.geojson.Feature;

/** OpenMapTiles capital is an admin level, not a boolean; names vary by country. */
final class CapitalStyle {
  static final String STYLE_URL = "https://tiles.openfreemap.org/styles/positron";
  static final String MARKERS = "ngon-capitals";
  static final String LABELS = "ngon-capital-labels";
  static final String FILTER = "[\"all\",[\">=\",[\"to-number\",[\"get\",\"capital\"],0],2],"
      + "[\"<=\",[\"to-number\",[\"get\",\"capital\"],0],6],"
      + "[\"match\",[\"get\",\"class\"],[\"city\",\"town\",\"village\",\"hamlet\",\"borough\"],true,false]]";

  static String compose(String baseJson, String overlayJson) throws JSONException {
    JSONObject base = new JSONObject(baseJson);
    JSONArray layers = base.getJSONArray("layers");
    String source = null;
    for (int index = 0; index < layers.length(); index++) {
      JSONObject layer = layers.getJSONObject(index);
      if (!"place".equals(layer.optString("source-layer"))) continue;
      source = layer.getString("source");
      if ("symbol".equals(layer.optString("type"))) {
        JSONArray filter = new JSONArray().put("all");
        if (layer.has("filter")) filter.put(layer.getJSONArray("filter"));
        // Suppress the provider's capital dots/labels, but retain ordinary place labels.
        filter.put(new JSONArray().put("!").put(new JSONArray(FILTER)));
        layer.put("filter", filter);
      }
    }
    if (source == null || !"vector".equals(base.getJSONObject("sources").getJSONObject(source).optString("type"))) {
      throw new JSONException("OpenFreeMap style has no vector place source");
    }
    JSONArray overlays = new JSONArray(overlayJson);
    for (int index = 0; index < overlays.length(); index++) {
      JSONObject layer = overlays.getJSONObject(index);
      layer.put("source", source).put("source-layer", "place").put("filter", new JSONArray(FILTER));
      layers.put(layer);
    }
    base.put("name", "Capital Atlas · OpenFreeMap + n-gon");
    // Camera presets belong to the activity, independent of upstream style defaults.
    base.remove("center");
    base.remove("zoom");
    return base.toString();
  }

  static int level(Feature feature) {
    try { return feature.getProperty("capital").getAsInt(); }
    catch (RuntimeException ignored) { return 0; }
  }

  static String name(Feature feature) {
    for (String key : new String[]{"name:en", "name_en", "name:latin", "name"}) {
      if (feature.hasNonNullValueForProperty(key)) return feature.getStringProperty(key);
    }
    return "Unnamed place";
  }

  private CapitalStyle() {}
}
