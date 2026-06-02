#ifndef FONT3X5_H
#define FONT3X5_H

#include <Arduino.h>

// 3x5 pixel font — each character is 5 rows, 3 columns.
// Each row byte uses bits 2..0 (bit2=leftmost pixel).
// Indexed by FONT_INDEX[] mapping below.

#define FONT_CHAR_W  3
#define FONT_CHAR_H  5

static const uint8_t FONT_DATA[][FONT_CHAR_H] PROGMEM = {
  // 0: Space
  {0b000, 0b000, 0b000, 0b000, 0b000},
  // 1: '0'
  {0b111, 0b101, 0b101, 0b101, 0b111},
  // 2: '1'
  {0b010, 0b110, 0b010, 0b010, 0b111},
  // 3: '2'
  {0b111, 0b001, 0b111, 0b100, 0b111},
  // 4: '3'
  {0b111, 0b001, 0b111, 0b001, 0b111},
  // 5: '4'
  {0b101, 0b101, 0b111, 0b001, 0b001},
  // 6: '5'
  {0b111, 0b100, 0b111, 0b001, 0b111},
  // 7: '6'
  {0b111, 0b100, 0b111, 0b101, 0b111},
  // 8: '7'
  {0b111, 0b001, 0b010, 0b010, 0b010},
  // 9: '8'
  {0b111, 0b101, 0b111, 0b101, 0b111},
  // 10: '9'
  {0b111, 0b101, 0b111, 0b001, 0b111},
  // 11: ','
  {0b000, 0b000, 0b000, 0b010, 0b100},
  // 12: 'A'
  {0b010, 0b101, 0b111, 0b101, 0b101},
  // 13: 'B'
  {0b110, 0b101, 0b110, 0b101, 0b110},
  // 14: 'C'
  {0b011, 0b100, 0b100, 0b100, 0b011},
  // 15: 'D'
  {0b110, 0b101, 0b101, 0b101, 0b110},
  // 16: 'E'
  {0b111, 0b100, 0b110, 0b100, 0b111},
  // 17: 'F'
  {0b111, 0b100, 0b110, 0b100, 0b100},
  // 18: 'G'
  {0b011, 0b100, 0b101, 0b101, 0b011},
  // 19: 'H'
  {0b101, 0b101, 0b111, 0b101, 0b101},
  // 20: 'I'
  {0b111, 0b010, 0b010, 0b010, 0b111},
  // 21: 'J'
  {0b001, 0b001, 0b001, 0b101, 0b010},
  // 22: 'K'
  {0b101, 0b101, 0b110, 0b101, 0b101},
  // 23: 'L'
  {0b100, 0b100, 0b100, 0b100, 0b111},
  // 24: 'M'
  {0b101, 0b111, 0b111, 0b101, 0b101},
  // 25: 'N'
  {0b101, 0b111, 0b111, 0b111, 0b101},
  // 26: 'O'
  {0b010, 0b101, 0b101, 0b101, 0b010},
  // 27: 'P'
  {0b110, 0b101, 0b110, 0b100, 0b100},
  // 28: 'Q'
  {0b010, 0b101, 0b101, 0b110, 0b011},
  // 29: 'R'
  {0b110, 0b101, 0b110, 0b101, 0b101},
  // 30: 'S'
  {0b011, 0b100, 0b010, 0b001, 0b110},
  // 31: 'T'
  {0b111, 0b010, 0b010, 0b010, 0b010},
  // 32: 'U'
  {0b101, 0b101, 0b101, 0b101, 0b010},
  // 33: 'V'
  {0b101, 0b101, 0b101, 0b010, 0b010},
  // 34: 'W'
  {0b101, 0b101, 0b111, 0b111, 0b101},
  // 35: 'X'
  {0b101, 0b101, 0b010, 0b101, 0b101},
  // 36: 'Y'
  {0b101, 0b101, 0b010, 0b010, 0b010},
  // 37: 'Z'
  {0b111, 0b001, 0b010, 0b100, 0b111},
  // 38: '.'
  {0b000, 0b000, 0b000, 0b000, 0b010},
  // 39: '-'
  {0b000, 0b000, 0b111, 0b000, 0b000},
  // 40: '!'
  {0b010, 0b010, 0b010, 0b000, 0b010},
  // 41: '?'
  {0b110, 0b001, 0b010, 0b000, 0b010},
};

#define FONT_COUNT  (sizeof(FONT_DATA) / sizeof(FONT_DATA[0]))

// Charsets (must match original SplitFlap for animation cycling)
static const char SF_CHARSET_NUM[]  = " 0123456789,";
static const uint8_t SF_CHARSET_NUM_LEN = sizeof(SF_CHARSET_NUM) - 1;
static const char SF_CHARSET_FULL[] = " 0123456789,ABCDEFGHIJKLMNOPQRSTUVWXYZ.-!?";
static const uint8_t SF_CHARSET_FULL_LEN = sizeof(SF_CHARSET_FULL) - 1;

// Map ASCII char to FONT_DATA index. Returns 0 (space) for unknown.
static uint8_t charToFontIndex(char ch) {
  if (ch == ' ')  return 0;
  if (ch >= '0' && ch <= '9') return 1 + (ch - '0');
  if (ch == ',')  return 11;
  if (ch >= 'A' && ch <= 'Z') return 12 + (ch - 'A');
  if (ch >= 'a' && ch <= 'z') return 12 + (ch - 'a');  // lowercase → uppercase
  if (ch == '.')  return 38;
  if (ch == '-')  return 39;
  if (ch == '!')  return 40;
  if (ch == '?')  return 41;
  return 0;
}

// Read one row of a glyph from PROGMEM
static inline uint8_t fontRow(uint8_t fontIdx, uint8_t row) {
  return pgm_read_byte(&FONT_DATA[fontIdx][row]);
}

#endif // FONT3X5_H
