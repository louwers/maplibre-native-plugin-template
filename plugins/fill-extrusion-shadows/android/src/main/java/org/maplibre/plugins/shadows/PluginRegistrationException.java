package org.maplibre.plugins.shadows;

/** Thrown when the native host rejects the plugin descriptor. */
public final class PluginRegistrationException extends RuntimeException {
  private final int status;

  PluginRegistrationException(int status, String message) {
    super(message);
    this.status = status;
  }

  public int getStatus() {
    return status;
  }
}

