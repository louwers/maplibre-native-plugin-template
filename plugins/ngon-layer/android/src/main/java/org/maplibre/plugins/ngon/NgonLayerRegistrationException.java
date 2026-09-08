package org.maplibre.plugins.ngon;

/** Raised when MapLibre rejects the ngon layer descriptor. */
public final class NgonLayerRegistrationException extends RuntimeException {
  private final int status;

  NgonLayerRegistrationException(int status, String message) {
    super(message == null || message.isEmpty() ? "MapLibre rejected the ngon layer plugin" : message);
    this.status = status;
  }

  public int getStatus() {
    return status;
  }
}
