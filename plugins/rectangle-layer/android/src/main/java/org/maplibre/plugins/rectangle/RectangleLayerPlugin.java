package org.maplibre.plugins.rectangle;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/** Registers the source-bound {@code rectangle} style layer. */
public final class RectangleLayerPlugin {
  public static final String ID = "org.maplibre.rectangle-layer";
  public static final String LAYER_TYPE = "rectangle";

  public enum RegistrationResult {
    REGISTERED,
    ALREADY_REGISTERED
  }

  private static boolean nativeLoaded;

  private RectangleLayerPlugin() {}

  public static synchronized RegistrationResult register() {
    long registrationFunctionAddress = registrationFunctionAddress();
    if (!nativeLoaded) {
      System.loadLibrary("rectangle-layer");
      nativeLoaded = true;
    }
    NativeResult result = nativeRegister(registrationFunctionAddress);
    if (result.status == 0) return RegistrationResult.REGISTERED;
    if (result.status == 1) return RegistrationResult.ALREADY_REGISTERED;
    throw new RectangleLayerRegistrationException(result.status, result.message);
  }

  private static long registrationFunctionAddress() {
    try {
      Class<?> registry = Class.forName("org.maplibre.android.plugins.MapLibrePluginRegistry");
      Method ensureLoaded = registry.getMethod("ensureMapLibreLoaded");
      Method address = registry.getMethod("registrationFunctionAddress");
      ensureLoaded.invoke(null);
      return ((Number) address.invoke(null)).longValue();
    } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException error) {
      throw new IllegalStateException("The selected MapLibre renderer does not provide plugin ABI v1", error);
    } catch (InvocationTargetException error) {
      Throwable cause = error.getCause();
      if (cause instanceof RuntimeException) throw (RuntimeException) cause;
      if (cause instanceof Error) throw (Error) cause;
      throw new IllegalStateException("MapLibre plugin ABI initialization failed", cause);
    }
  }

  private static native NativeResult nativeRegister(long registrationFunctionAddress);

  static final class NativeResult {
    final int status;
    final String message;

    NativeResult(int status, String message) {
      this.status = status;
      this.message = message;
    }
  }
}
