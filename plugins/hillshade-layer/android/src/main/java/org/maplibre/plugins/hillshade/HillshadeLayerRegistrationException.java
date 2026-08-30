package org.maplibre.plugins.hillshade;

/** MapLibre rejected the native hillshade plugin descriptor. */
public final class HillshadeLayerRegistrationException extends RuntimeException {
  private final int status;

  HillshadeLayerRegistrationException(int status, String message) {
    super(message == null || message.isEmpty() ? "Hillshade plugin registration failed with status " + status : message);
    this.status = status;
  }

  public int getStatus() {
    return status;
  }
}
