package org.maplibre.plugins.gltf;

/** Indicates that MapLibre rejected the GLTF layer descriptor. */
public final class GltfLayerRegistrationException extends RuntimeException {
  private final int status;

  GltfLayerRegistrationException(int status, String message) {
    super(message == null || message.isEmpty() ? "GLTF layer registration failed with status " + status : message);
    this.status = status;
  }

  public int getStatus() {
    return status;
  }
}
