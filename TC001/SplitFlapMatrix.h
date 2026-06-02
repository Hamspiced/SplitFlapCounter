#ifndef SPLITFLAP_MATRIX_H
#define SPLITFLAP_MATRIX_H

#include <Arduino.h>
#include <FastLED.h>
#include "font3x5.h"

// Requires setPixel(int x, int y, CRGB color) to be defined before including.

#define SFM_MAX_CELLS   8
#define SFM_CELL_W      3       // character width in pixels
#define SFM_CELL_GAP    1       // gap between cells
#define SFM_CELL_PITCH  (SFM_CELL_W + SFM_CELL_GAP)  // 4
#define SFM_ANIM_FRAMES 4       // frames per character transition

struct SFMCell {
  char    current;
  char    target;
  int8_t  animFrame;    // -1 = idle, 0..SFM_ANIM_FRAMES-1 = flipping
  bool    dirty;
};

class SplitFlapMatrix {
public:
  // Colors — set these before drawing to customize appearance
  CRGB textColor    = CRGB(255, 180, 0);   // digit color
  CRGB dimTextColor = CRGB(80, 56, 0);     // dimmed text during flip
  CRGB panelTop     = CRGB(12, 10, 2);     // lighter top flap
  CRGB panelBot     = CRGB(8, 6, 1);       // darker bottom flap
  CRGB splitColor   = CRGB::Black;         // split line
  CRGB gapColor     = CRGB::Black;         // gap / background

  SplitFlapMatrix() : _numCells(0), _startX(0), _numericMode(true),
                      _seqCount(0), _seqPos(0), _flapped(false) {}

  void begin(uint8_t startX, uint8_t numCells) {
    _startX = startX;
    _numCells = (numCells > SFM_MAX_CELLS) ? SFM_MAX_CELLS : numCells;
    for (uint8_t i = 0; i < SFM_MAX_CELLS; i++)
      _cells[i] = {' ', ' ', -1, true};
    _seqCount = 0;
    _seqPos = 0;
  }

  // Set target text, right-justified (for numeric counts).
  // Cells flip right-to-left in cascade sequence.
  void setTarget(const char *text) {
    uint8_t len = strlen(text);
    uint8_t pad = (_numCells > len) ? _numCells - len : 0;
    _seqCount = 0;
    for (int8_t i = _numCells - 1; i >= 0; i--) {
      char ch = ((uint8_t)i < pad) ? ' ' : text[i - pad];
      _cells[i].target = ch;
      if (_cells[i].current != ch)
        _seqQueue[_seqCount++] = i;
    }
    _seqPos = 0;
    if (_seqCount > 0) {
      _cells[_seqQueue[0]].animFrame = 0;
      _cells[_seqQueue[0]].dirty = true;
    }
  }

  void setNumericMode(bool on) { _numericMode = on; }

  // Advance animation by one frame. Returns true if any cell is busy.
  bool update() {
    if (_seqPos >= _seqCount) return false;
    uint8_t idx = _seqQueue[_seqPos];
    SFMCell &c = _cells[idx];
    if (c.animFrame < 0) return false;

    c.animFrame++;
    c.dirty = true;

    if (c.animFrame >= SFM_ANIM_FRAMES) {
      c.current = _nextChar(c.current);
      c.animFrame = -1;
      _flapped = true;
      if (c.current == c.target) {
        // This cell is done — start the next one in cascade
        _seqPos++;
        if (_seqPos < _seqCount) {
          _cells[_seqQueue[_seqPos]].animFrame = 0;
          _cells[_seqQueue[_seqPos]].dirty = true;
        }
      } else {
        c.animFrame = 0;  // keep cycling
      }
    }
    return true;
  }

  // Render all dirty cells to the matrix via setPixel().
  void draw() {
    for (uint8_t i = 0; i < _numCells; i++) {
      if (!_cells[i].dirty) continue;
      _cells[i].dirty = false;
      _drawCell(i);
    }
  }

  // Force redraw of all cells.
  void drawAll() {
    for (uint8_t i = 0; i < _numCells; i++) _cells[i].dirty = true;
    draw();
  }

  bool isBusy() const { return _seqPos < _seqCount; }

  bool didFlap() {
    if (_flapped) { _flapped = false; return true; }
    return false;
  }

  uint8_t numCells() const { return _numCells; }

private:
  SFMCell   _cells[SFM_MAX_CELLS];
  uint8_t   _numCells;
  uint8_t   _startX;
  bool      _numericMode;
  bool      _flapped;
  uint8_t   _seqQueue[SFM_MAX_CELLS];
  uint8_t   _seqCount, _seqPos;

  char _nextChar(char ch) {
    const char *cs  = _numericMode ? SF_CHARSET_NUM  : SF_CHARSET_FULL;
    uint8_t     len = _numericMode ? SF_CHARSET_NUM_LEN : SF_CHARSET_FULL_LEN;
    for (uint8_t i = 0; i < len; i++)
      if (cs[i] == ch) return cs[(i + 1) % len];
    return cs[0];
  }

  // Render one cell at its matrix position.
  // Layout per cell (8 rows):
  //   Row 0-2 : font rows 0-2 on lighter panel bg (top flap)
  //   Row 3   : split line (black)
  //   Row 4-5 : font rows 3-4 on darker panel bg (bottom flap)
  //   Row 6   : bottom panel border (dim)
  //   Row 7   : gap (black)
  void _drawCell(uint8_t cellIdx) {
    int16_t cx = _startX + cellIdx * SFM_CELL_PITCH;
    char ch = _cells[cellIdx].current;
    uint8_t fi = charToFontIndex(ch);
    bool flipping = (_cells[cellIdx].animFrame >= 0);
    CRGB textCol = flipping ? dimTextColor : textColor;

    for (uint8_t col = 0; col < SFM_CELL_W; col++) {
      int16_t px = cx + col;
      // Rows 0-2: top flap with font rows 0-2
      for (uint8_t fr = 0; fr < 3; fr++) {
        bool lit = (fontRow(fi, fr) >> (2 - col)) & 1;
        setPixel(px, fr, lit ? textCol : panelTop);
      }
      // Row 3: split line
      setPixel(px, 3, splitColor);
      // Rows 4-5: bottom flap with font rows 3-4
      for (uint8_t fr = 3; fr < FONT_CHAR_H; fr++) {
        bool lit = (fontRow(fi, fr) >> (2 - col)) & 1;
        setPixel(px, fr + 1, lit ? textCol : panelBot);
      }
      // Row 6: bottom border
      setPixel(px, 6, panelBot);
      // Row 7: gap
      setPixel(px, 7, gapColor);
    }
  }
};

#endif // SPLITFLAP_MATRIX_H
