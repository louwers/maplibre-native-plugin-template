package org.maplibre.plugins.shadows;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

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
    long registrationFunctionAddress = registrationFunctionAddress();
    if (!nativeLoaded) {
      System.loadLibrary("fill-extrusion-shadows");
      nativeLoaded = true;
    }
    NativeResult result = nativeRegister(registrationFunctionAddress);
    if (result.status == 0) {
      return RegistrationResult.REGISTERED;
    }
    if (result.status == 1) {
      return RegistrationResult.ALREADY_REGISTERED;
    }
    throw new PluginRegistrationException(result.status, result.message);
  }

  private static long registrationFunctionAddress() {
    try {
      Class<?> registry = Class.forName("org.maplibre.android.plugins.MapLibrePluginRegistry");
      Method ensureLoaded = registry.getMethod("ensureMapLibreLoaded");
      Method address = registry.getMethod("registrationFunctionAddress");
      ensureLoaded.invoke(null);
      return ((Number) address.invoke(null)).longValue();
    } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException error) {
      throw new IllegalStateException(
          "The selected MapLibre renderer does not provide plugin ABI v1", error);
    } catch (InvocationTargetException error) {
      Throwable cause = error.getCause();
      if (cause instanceof RuntimeException) {
        throw (RuntimeException) cause;
      }
      if (cause instanceof Error) {
        throw (Error) cause;
      }
      throw new IllegalStateException("MapLibre plugin ABI initialization failed", cause);
    }
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
