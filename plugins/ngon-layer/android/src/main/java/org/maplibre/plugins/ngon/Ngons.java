package org.maplibre.plugins.ngon;

import org.maplibre.android.style.expressions.Expression;
import org.maplibre.android.style.layers.PaintPropertyValue;

/** Constant and expression helpers; usable through any MapLibre Layer wrapper. */
public final class Ngons {
  private Ngons() {}

  public static PaintPropertyValue<Float> radius(float value) {
    return new PaintPropertyValue<>("ngon-radius", value);
  }

  public static PaintPropertyValue<Expression> radius(Expression value) {
    return new PaintPropertyValue<>("ngon-radius", value);
  }

  public static PaintPropertyValue<Float> corners(float value) {
    return new PaintPropertyValue<>("ngon-corners", value);
  }

  public static PaintPropertyValue<Expression> corners(Expression value) {
    return new PaintPropertyValue<>("ngon-corners", value);
  }

  public static PaintPropertyValue<Float> rotate(float value) {
    return new PaintPropertyValue<>("ngon-rotate", value);
  }

  public static PaintPropertyValue<Expression> rotate(Expression value) {
    return new PaintPropertyValue<>("ngon-rotate", value);
  }

  public static PaintPropertyValue<String> color(String value) {
    return new PaintPropertyValue<>("ngon-color", value);
  }

  public static PaintPropertyValue<Expression> color(Expression value) {
    return new PaintPropertyValue<>("ngon-color", value);
  }

  public static PaintPropertyValue<Float> blur(float value) {
    return new PaintPropertyValue<>("ngon-blur", value);
  }

  public static PaintPropertyValue<Expression> blur(Expression value) {
    return new PaintPropertyValue<>("ngon-blur", value);
  }

  public static PaintPropertyValue<Float> opacity(float value) {
    return new PaintPropertyValue<>("ngon-opacity", value);
  }

  public static PaintPropertyValue<Expression> opacity(Expression value) {
    return new PaintPropertyValue<>("ngon-opacity", value);
  }

  public static PaintPropertyValue<Float> strokeWidth(float value) {
    return new PaintPropertyValue<>("ngon-stroke-width", value);
  }

  public static PaintPropertyValue<Expression> strokeWidth(Expression value) {
    return new PaintPropertyValue<>("ngon-stroke-width", value);
  }

  public static PaintPropertyValue<String> strokeColor(String value) {
    return new PaintPropertyValue<>("ngon-stroke-color", value);
  }

  public static PaintPropertyValue<Expression> strokeColor(Expression value) {
    return new PaintPropertyValue<>("ngon-stroke-color", value);
  }

  public static PaintPropertyValue<Float> strokeOpacity(float value) {
    return new PaintPropertyValue<>("ngon-stroke-opacity", value);
  }

  public static PaintPropertyValue<Expression> strokeOpacity(Expression value) {
    return new PaintPropertyValue<>("ngon-stroke-opacity", value);
  }

  public static PaintPropertyValue<Float[]> translate(Float[] value) {
    return new PaintPropertyValue<>("ngon-translate", value);
  }

  public static PaintPropertyValue<Expression> translate(Expression value) {
    return new PaintPropertyValue<>("ngon-translate", value);
  }

  public static PaintPropertyValue<String> translateAnchor(String value) {
    return new PaintPropertyValue<>("ngon-translate-anchor", value);
  }

  public static PaintPropertyValue<Expression> translateAnchor(Expression value) {
    return new PaintPropertyValue<>("ngon-translate-anchor", value);
  }

  public static PaintPropertyValue<String> pitchAlignment(String value) {
    return new PaintPropertyValue<>("ngon-pitch-alignment", value);
  }

  public static PaintPropertyValue<Expression> pitchAlignment(Expression value) {
    return new PaintPropertyValue<>("ngon-pitch-alignment", value);
  }

  public static PaintPropertyValue<String> pitchScale(String value) {
    return new PaintPropertyValue<>("ngon-pitch-scale", value);
  }

  public static PaintPropertyValue<Expression> pitchScale(Expression value) {
    return new PaintPropertyValue<>("ngon-pitch-scale", value);
  }
}
