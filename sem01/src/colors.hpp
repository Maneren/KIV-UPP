/* Adapted from my project for the Introduction to Computer Graphics course. */

struct Color {
  unsigned char r, g, b, a;
};

/**
 * @brief Linearly interpolate between two colors (optimized interface).
 *
 * @param a first color (0)
 * @param b second color (1000)
 * @param t interpolation factor (0..1000)
 *
 * @return Interpolated color
 *
 * Around 8% faster that a full float version with <1% error.
 */
inline Color lerpColor(const Color &a, const Color &b, int t) {
  return {static_cast<unsigned char>(a.r + t * (b.r - a.r) / 1000),
          static_cast<unsigned char>(a.g + t * (b.g - a.g) / 1000),
          static_cast<unsigned char>(a.b + t * (b.b - a.b) / 1000), 255};
}

/**
 * @brief Linearly interpolate between two colors
 *
 * @param a first color (0.0)
 * @param b second color (1.0)
 * @param t interpolation factor (0.0..1.0)
 *
 * @return Interpolated color
 *
 * Uses `lerpColor(Color, Color, int)` internally (converting `t` to `int`) for
 * performance.
 */
inline Color lerpColor(const Color &a, const Color &b, float t) {
  return lerpColor(a, b, static_cast<int>(t * 1000.f));
}

/**
 * @brief Interpolate between three colors
 *
 * @param a first color (-1.0)
 * @param b middle color (0.0)
 * @param c last color (1.0)
 * @param t interpolation factor (-1.0..1.0)
 *
 * @return Interpolated color
 *
 * Uses `lerpColor(Color, Color, int)` internally (converting `t` to `int`) for
 * performance.
 */
inline Color lerpColor3(const Color &a, const Color &b, const Color &c,
                        float t) {
  auto t_int = static_cast<int>(t * 1000.f);

  if (t_int <= 0) {
    return lerpColor(a, b, t_int + 1000);
  } else {
    return lerpColor(b, c, t_int);
  }
}
