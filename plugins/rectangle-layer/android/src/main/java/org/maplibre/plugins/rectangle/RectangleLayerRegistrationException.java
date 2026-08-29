package org.maplibre.plugins.rectangle;

/** Raised when MapLibre rejects the rectangle layer descriptor. */
public final class RectangleLayerRegistrationException extends RuntimeException {
  private final int status;

  RectangleLayerRegistrationException(int status, String message) {
    super(message == null || message.isEmpty() ? "MapLibre rejected the rectangle layer plugin" : message);
    this.status = status;
  }

  public int getStatus() {
    return status;
  }
}
