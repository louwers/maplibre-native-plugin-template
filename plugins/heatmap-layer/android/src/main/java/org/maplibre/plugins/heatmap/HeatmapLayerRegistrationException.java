package org.maplibre.plugins.heatmap;

/** MapLibre rejected the native heatmap plugin descriptor. */
public final class HeatmapLayerRegistrationException extends RuntimeException {
  private final int status;

  HeatmapLayerRegistrationException(int status, String message) {
    super(message == null || message.isEmpty() ? "Heatmap plugin registration failed with status " + status : message);
    this.status = status;
  }

  public int getStatus() {
    return status;
  }
}
