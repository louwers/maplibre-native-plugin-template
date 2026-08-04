package org.maplibre.plugins.shadows;

import org.maplibre.android.plugins.MapLibrePluginRegistry;

/** Loads and registers the native fill-extrusion shadow extension. */
public final class FillExtrusionShadowsPlugin {
  public static final String ID = "org.maplibre.fill-extrusion-shadows";

  public enum RegistrationResult {
    REGISTERED,
    ALREADY_REGISTERED
  }

  private static boolean nativeLoaded;

  private FillExtrusionShadowsPlugin() {
  }

  public static synchronized RegistrationResult register() {
    MapLibrePluginRegistry.ensureMapLibreLoaded();
    if (!nativeLoaded) {
      System.loadLibrary("fill-extrusion-shadows");
      nativeLoaded = true;
    }
    NativeResult result = nativeRegister(MapLibrePluginRegistry.registrationFunctionAddress());
    if (result.status == 0) {
      return RegistrationResult.REGISTERED;
    }
    if (result.status == 1) {
      return RegistrationResult.ALREADY_REGISTERED;
    }
    throw new PluginRegistrationException(result.status, result.message);
  }

  /** Returns the number of enabled shadow composites completed by the native plugin. */
  public static long renderCallbackCount() {
    return nativeRenderCallbackCount();
  }

  private static native NativeResult nativeRegister(long registrationFunctionAddress);
  private static native long nativeRenderCallbackCount();

  /** Internal JNI result carrier. */
  static final class NativeResult {
    final int status;
    final String message;

    NativeResult(int status, String message) {
      this.status = status;
      this.message = message;
    }
  }
}
