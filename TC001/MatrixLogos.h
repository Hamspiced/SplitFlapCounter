#ifndef MATRIX_LOGOS_H
#define MATRIX_LOGOS_H

#include <Arduino.h>
#include <FastLED.h>

// 8x8 pixel art icons stored row-major (row 0 left-to-right, then row 1, etc.)
// Uses PROGMEM-friendly packed format: each pixel is a palette index.
// 0 = off/black, 1 = primary color, 2 = white accent

#define ICON_SIZE 8

// YouTube: red rounded rectangle with white play triangle
// Rows 0-6 visible, row 7 kept black to match split-flap cell gap
//   _ R R R R R R _
//   R R R W R R R R
//   R R R W W R R R
//   R R R W W W R R
//   R R R W W R R R
//   R R R W R R R R
//   _ R R R R R R _
//   _ _ _ _ _ _ _ _
static const uint8_t YT_ICON[ICON_SIZE * ICON_SIZE] PROGMEM = {
  0,1,1,1,1,1,1,0,
  1,1,1,2,1,1,1,1,
  1,1,1,2,2,1,1,1,
  1,1,1,2,2,2,1,1,
  1,1,1,2,2,1,1,1,
  1,1,1,2,1,1,1,1,
  0,1,1,1,1,1,1,0,
  0,0,0,0,0,0,0,0,
};

// Instagram: rounded square with lens circle + flash dot (upper-right)
// Better matches the real IG glyph at 8x8 resolution
// Rows 0-6 visible, row 7 kept black
//   _ P P P P P P _
//   P P _ _ _ P W P
//   P _ _ W W _ _ P
//   P _ W _ _ W _ P
//   P _ W _ _ W _ P
//   P _ _ W W _ _ P
//   _ P P P P P P _
//   _ _ _ _ _ _ _ _
static const uint8_t IG_ICON[ICON_SIZE * ICON_SIZE] PROGMEM = {
  0,1,1,1,1,1,1,0,
  1,1,0,0,0,1,2,1,
  1,0,0,2,2,0,0,1,
  1,0,2,0,0,2,0,1,
  1,0,2,0,0,2,0,1,
  1,0,0,2,2,0,0,1,
  0,1,1,1,1,1,1,0,
  0,0,0,0,0,0,0,0,
};

// Draw an 8x8 icon at position (x, y) on the matrix.
// setPixelFn must be: void(int x, int y, CRGB color)
// primary/accent are the two palette colors, bg fills empty pixels.
template <typename SetPixelFn>
static void drawIcon(SetPixelFn setPixelFn, const uint8_t *icon,
                     int16_t ox, int16_t oy, CRGB primary, CRGB accent, CRGB bg = CRGB::Black) {
  for (uint8_t row = 0; row < ICON_SIZE; row++) {
    for (uint8_t col = 0; col < ICON_SIZE; col++) {
      uint8_t idx = pgm_read_byte(&icon[row * ICON_SIZE + col]);
      CRGB c;
      if (idx == 1) c = primary;
      else if (idx == 2) c = accent;
      else c = bg;
      setPixelFn(ox + col, oy + row, c);
    }
  }
}

#endif // MATRIX_LOGOS_H
