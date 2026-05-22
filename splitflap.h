#ifndef SPLITFLAP_H
#define SPLITFLAP_H

#include <TFT_eSPI.h>

// =====================================================================
//  SplitFlap – mechanical split-flap display emulation on TFT
//
//  Cell dimensions are configurable per instance via begin().
//  Each cell cycles through the charset sequentially right-to-left.
// =====================================================================

#define SF_MAX_CHARS    15      // max cells per instance
#define SF_SPLIT_H      2
#define SF_CORNER_R     3

// --- Colors (RGB565) ---
#define SF_BG           0x1082
#define SF_PANEL        0x2945
#define SF_PANEL_LIGHT  0x3186
#define SF_TEXT          0xFFE0
#define SF_SPLIT_COLOR  0x0000

// --- Animation ---
#define SF_ANIM_FRAMES  16

// Two charsets
static const char SF_CHARSET_NUM[]  = " 0123456789,";
static const uint8_t SF_CHARSET_NUM_LEN = sizeof(SF_CHARSET_NUM) - 1;
static const char SF_CHARSET_FULL[] = " 0123456789,ABCDEFGHIJKLMNOPQRSTUVWXYZ.-!?";
static const uint8_t SF_CHARSET_FULL_LEN = sizeof(SF_CHARSET_FULL) - 1;

struct SplitFlapCell {
  char    current;
  char    target;
  int8_t  animFrame;
  bool    dirty;
};

class SplitFlap {
public:
  SplitFlap() : _numCells(0), _x(0), _y(0), _cellW(30), _cellH(50), _gap(4),
                _tft(nullptr), _numericMode(false), _useSmallFont(false),
                _seqCount(0), _seqPos(0), _flapped(false), _simultaneous(false) {}

  // cellW/cellH: pixel dimensions of each cell. gap: space between cells.
  // smallFont: true to use 9pt font (for name row), false for 18pt (number row).
  void begin(TFT_eSPI *tft, int16_t x, int16_t y, uint8_t numCells,
             uint8_t cellW = 30, uint8_t cellH = 50, uint8_t gap = 4, bool smallFont = false) {
    _tft = tft;
    _x = x;  _y = y;
    _cellW = cellW;  _cellH = cellH;  _gap = gap;
    _useSmallFont = smallFont;
    _numCells = (numCells > SF_MAX_CHARS) ? SF_MAX_CHARS : numCells;
    for (uint8_t i = 0; i < SF_MAX_CHARS; i++)
      _cells[i] = { ' ', ' ', -1, true };
    _seqCount = 0;  _seqPos = 0;
  }

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

  // Left-justify variant (for name row)
  void setTargetLeft(const char *text) {
    uint8_t len = strlen(text);
    _seqCount = 0;
    for (int8_t i = _numCells - 1; i >= 0; i--) {
      char ch = ((uint8_t)i < len) ? text[i] : ' ';
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

  // All-at-once left-justified: every cell starts flipping simultaneously.
  // Each cell independently cycles through the charset to its target.
  void setTargetLeftAllAtOnce(const char *text) {
    uint8_t len = strlen(text);
    _simultaneous = true;
    for (uint8_t i = 0; i < _numCells; i++) {
      char ch = (i < len) ? text[i] : ' ';
      _cells[i].target = ch;
      if (_cells[i].current != ch) {
        _cells[i].animFrame = 0;
        _cells[i].dirty = true;
      }
    }
    // Queue not used in simultaneous mode, but mark as busy
    _seqCount = 1; _seqPos = 0;
  }

  void setNumericMode(bool on) { _numericMode = on; }

  bool update() {
    if (_simultaneous) return _updateSimultaneous();
    if (_seqPos >= _seqCount) return false;
    uint8_t idx = _seqQueue[_seqPos];
    SplitFlapCell &c = _cells[idx];
    if (c.animFrame < 0) return false;
    c.animFrame++;
    c.dirty = true;
    if (c.animFrame >= SF_ANIM_FRAMES) {
      c.current = _nextChar(c.current);
      c.animFrame = -1;
      _flapped = true;
      if (c.current == c.target) {
        _seqPos++;
        if (_seqPos < _seqCount) {
          _cells[_seqQueue[_seqPos]].animFrame = 0;
          _cells[_seqQueue[_seqPos]].dirty = true;
        }
      } else {
        c.animFrame = 0;
      }
    }
    return true;
  }

  void draw() {
    if (!_tft) return;
    TFT_eSprite spr(_tft);
    spr.createSprite(_cellW, _cellH);
    spr.setTextDatum(MC_DATUM);
    for (uint8_t i = 0; i < _numCells; i++) {
      if (!_cells[i].dirty) continue;
      _cells[i].dirty = false;
      int16_t cx = _x + i * (_cellW + _gap);
      _renderCell(spr, _cells[i]);
      spr.pushSprite(cx, _y);
    }
    spr.deleteSprite();
  }

  void drawAll() {
    for (uint8_t i = 0; i < _numCells; i++) _cells[i].dirty = true;
    draw();
  }

  uint16_t totalWidth() const { return _numCells * (_cellW + _gap) - _gap; }
  uint8_t  numCells()   const { return _numCells; }
  bool isBusy() const {
    if (_simultaneous) {
      for (uint8_t i = 0; i < _numCells; i++)
        if (_cells[i].current != _cells[i].target || _cells[i].animFrame >= 0) return true;
      return false;
    }
    return _seqPos < _seqCount;
  }

  bool didFlap() {
    if (_flapped) { _flapped = false; return true; }
    return false;
  }

private:
  SplitFlapCell _cells[SF_MAX_CHARS];
  uint8_t  _numCells;
  int16_t  _x, _y;
  uint8_t  _cellW, _cellH, _gap;
  TFT_eSPI *_tft;
  bool     _numericMode;
  bool     _useSmallFont;
  bool     _flapped;
  bool     _simultaneous;   // true = all cells flip at once
  uint8_t  _seqQueue[SF_MAX_CHARS];
  uint8_t  _seqCount, _seqPos;

  // Simultaneous mode: all cells animate independently at the same time
  bool _updateSimultaneous() {
    bool busy = false;
    for (uint8_t i = 0; i < _numCells; i++) {
      SplitFlapCell &c = _cells[i];
      if (c.animFrame >= 0) {
        c.animFrame++;
        c.dirty = true;
        if (c.animFrame >= SF_ANIM_FRAMES) {
          c.current = _nextChar(c.current);
          c.animFrame = -1;
          _flapped = true;
          if (c.current != c.target)
            c.animFrame = 0;  // keep flipping
        }
        busy = true;
      }
    }
    if (!busy) _simultaneous = false;  // done, reset mode
    return busy;
  }

  char _nextChar(char ch) {
    const char *cs  = _numericMode ? SF_CHARSET_NUM  : SF_CHARSET_FULL;
    uint8_t     len = _numericMode ? SF_CHARSET_NUM_LEN : SF_CHARSET_FULL_LEN;
    for (uint8_t i = 0; i < len; i++)
      if (cs[i] == ch) return cs[(i + 1) % len];
    return cs[0];
  }

  void _setFont(TFT_eSprite &spr) {
    if (_useSmallFont)
      spr.setFreeFont(&FreeSansBold9pt7b);
    else
      spr.setFreeFont(&FreeSansBold18pt7b);
  }

  void _renderCell(TFT_eSprite &spr, const SplitFlapCell &cell) {
    int16_t halfH = _cellH / 2;
    int16_t splitY = halfH - SF_SPLIT_H / 2;
    int16_t textY = _cellH / 2 + (_useSmallFont ? 1 : 2);

    if (cell.animFrame < 0) {
      spr.fillSprite(SF_BG);
      spr.fillRoundRect(0, 0, _cellW, _cellH, SF_CORNER_R, SF_PANEL);
      spr.fillRoundRect(0, 0, _cellW, halfH, SF_CORNER_R, SF_PANEL_LIGHT);
      spr.fillRect(0, halfH - SF_CORNER_R, SF_CORNER_R, SF_CORNER_R, SF_PANEL_LIGHT);
      spr.fillRect(_cellW - SF_CORNER_R, halfH - SF_CORNER_R, SF_CORNER_R, SF_CORNER_R, SF_PANEL_LIGHT);
      spr.fillRect(0, splitY, _cellW, SF_SPLIT_H, SF_SPLIT_COLOR);
      spr.setTextColor(SF_TEXT);
      _setFont(spr);
      spr.setTextDatum(MC_DATUM);
      char buf[2] = { cell.current, '\0' };
      spr.drawString(buf, _cellW / 2, textY);
      return;
    }

    float progress = (float)cell.animFrame / (float)SF_ANIM_FRAMES;
    char oldCh = cell.current;
    char newCh = _nextChar(cell.current);

    spr.fillSprite(SF_BG);
    spr.fillRoundRect(0, 0, _cellW, _cellH, SF_CORNER_R, SF_PANEL);
    spr.fillRoundRect(0, 0, _cellW, halfH, SF_CORNER_R, SF_PANEL_LIGHT);
    spr.fillRect(0, halfH - SF_CORNER_R, SF_CORNER_R, SF_CORNER_R, SF_PANEL_LIGHT);
    spr.fillRect(_cellW - SF_CORNER_R, halfH - SF_CORNER_R, SF_CORNER_R, SF_CORNER_R, SF_PANEL_LIGHT);

    spr.setTextColor(SF_TEXT);
    _setFont(spr);
    spr.setTextDatum(MC_DATUM);
    char nbuf[2] = { newCh, '\0' };
    spr.drawString(nbuf, _cellW / 2, textY);

    int16_t flapH = (int16_t)(halfH * (1.0f - progress));
    if (flapH > 0) {
      int16_t flapY = splitY - flapH + SF_SPLIT_H;
      if (flapY < 0) flapY = 0;
      spr.fillRect(0, flapY, _cellW, flapH, SF_PANEL_LIGHT);
      if (flapH > halfH / 2) {
        spr.setTextColor(SF_TEXT);
        _setFont(spr);
        spr.setTextDatum(MC_DATUM);
        int16_t yOff = (int16_t)(progress * (_useSmallFont ? 3 : 6));
        char obuf[2] = { oldCh, '\0' };
        spr.drawString(obuf, _cellW / 2, textY - yOff);
      }
    }

    spr.fillRect(0, splitY, _cellW, SF_SPLIT_H, SF_SPLIT_COLOR);
  }
};

#endif // SPLITFLAP_H
