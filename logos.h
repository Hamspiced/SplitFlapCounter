#ifndef LOGOS_H
#define LOGOS_H

#include <Arduino.h>

// =====================================================================
//  Platform logos rendered inside oversized splitflap panels.
//  Same charcoal panel + split line aesthetic as the digit cells,
//  but larger to hold the logo icon.
// =====================================================================

// Logo panel dimensions (larger than digit cells)
#define LOGO_PANEL_W  72
#define LOGO_PANEL_H  50      // same height as digit cells for vertical alignment

// Shared splitflap panel colors (must match splitflap.h)
#define LP_BG           0x1082
#define LP_PANEL        0x2945
#define LP_PANEL_LIGHT  0x3186
#define LP_SPLIT_COLOR  0x0000
#define LP_SPLIT_H      2
#define LP_CORNER_R     5

// Draw the splitflap panel frame (shared by both logos)
static void _drawLogoPanel(TFT_eSPI &tft, int16_t x, int16_t y) {
  int16_t halfH = LOGO_PANEL_H / 2;
  int16_t splitY = halfH - LP_SPLIT_H / 2;

  // Bottom half (darker)
  tft.fillRoundRect(x, y, LOGO_PANEL_W, LOGO_PANEL_H, LP_CORNER_R, LP_PANEL);
  // Top half (lighter)
  tft.fillRoundRect(x, y, LOGO_PANEL_W, halfH, LP_CORNER_R, LP_PANEL_LIGHT);
  // Fix top-half bottom corners (square at split)
  tft.fillRect(x, y + halfH - LP_CORNER_R, LP_CORNER_R, LP_CORNER_R, LP_PANEL_LIGHT);
  tft.fillRect(x + LOGO_PANEL_W - LP_CORNER_R, y + halfH - LP_CORNER_R, LP_CORNER_R, LP_CORNER_R, LP_PANEL_LIGHT);
  // Split line
  tft.fillRect(x, y + splitY, LOGO_PANEL_W, LP_SPLIT_H, LP_SPLIT_COLOR);
}

// =====================================================================
//  YouTube Play Button inside a splitflap panel
// =====================================================================
static void drawYouTubeLogo(TFT_eSPI &tft, int16_t x, int16_t y) {
  const uint16_t red   = 0xF800;
  const uint16_t white = 0xFFFF;

  // Clear area and draw panel frame
  tft.fillRect(x, y, LOGO_PANEL_W, LOGO_PANEL_H, LP_BG);
  _drawLogoPanel(tft, x, y);

  // Red rounded rectangle (play button body) centered in panel
  int16_t bx = x + (LOGO_PANEL_W - 52) / 2;
  int16_t by = y + (LOGO_PANEL_H - 30) / 2;
  tft.fillRoundRect(bx, by, 52, 30, 6, red);

  // White play triangle centered in the red rect
  int16_t cx = bx + 26;
  int16_t cy = by + 15;
  tft.fillTriangle(
    cx - 7, cy - 9,
    cx - 7, cy + 9,
    cx + 9, cy,
    white
  );

  // Redraw split line on top of logo
  int16_t splitY = LOGO_PANEL_H / 2 - LP_SPLIT_H / 2;
  tft.fillRect(x, y + splitY, LOGO_PANEL_W, LP_SPLIT_H, LP_SPLIT_COLOR);
}

// =====================================================================
//  Instagram Camera Icon inside a splitflap panel
// =====================================================================
static void drawInstagramLogo(TFT_eSPI &tft, int16_t x, int16_t y) {
  // Clear area and draw panel frame
  tft.fillRect(x, y, LOGO_PANEL_W, LOGO_PANEL_H, LP_BG);
  _drawLogoPanel(tft, x, y);

  // IG gradient square centered in panel
  int16_t iconSz = 38;
  int16_t ix = x + (LOGO_PANEL_W - iconSz) / 2;
  int16_t iy = y + (LOGO_PANEL_H - iconSz) / 2;

  // Gradient bands (top-to-bottom: yellow → pink → purple)
  uint16_t grad[] = { 0xFECA, 0xFBE0, 0xD14E, 0xD14E, 0x9979, 0x4AFA };
  int bandH = iconSz / 6;
  for (int i = 0; i < 6; i++) {
    int16_t by = iy + i * bandH;
    int h = (i == 5) ? (iconSz - i * bandH) : bandH;
    tft.fillRect(ix + 3, by, iconSz - 6, h, grad[i]);
  }

  // Round the IG square corners by masking with panel color
  tft.fillRect(ix, iy, iconSz, 3, LP_PANEL_LIGHT);   // top edge in lighter panel
  tft.fillRect(ix, iy + iconSz - 3, iconSz, 3, LP_PANEL);  // bottom edge
  tft.fillRect(ix, iy, 3, iconSz, LP_PANEL_LIGHT);
  tft.fillRect(ix + iconSz - 3, iy, 3, iconSz, LP_PANEL);
  // Corner clips
  tft.fillRect(ix + 3, iy + 3, 2, 1, LP_PANEL_LIGHT);
  tft.fillRect(ix + 3, iy + 3, 1, 2, LP_PANEL_LIGHT);
  tft.fillRect(ix + iconSz - 5, iy + 3, 2, 1, LP_PANEL_LIGHT);
  tft.fillRect(ix + iconSz - 4, iy + 3, 1, 2, LP_PANEL_LIGHT);
  tft.fillRect(ix + 3, iy + iconSz - 4, 2, 1, LP_PANEL);
  tft.fillRect(ix + 3, iy + iconSz - 5, 1, 2, LP_PANEL);
  tft.fillRect(ix + iconSz - 5, iy + iconSz - 4, 2, 1, LP_PANEL);
  tft.fillRect(ix + iconSz - 4, iy + iconSz - 5, 1, 2, LP_PANEL);

  // Camera circle (white ring)
  int16_t ccx = ix + iconSz / 2;
  int16_t ccy = iy + iconSz / 2;
  tft.drawCircle(ccx, ccy, 10, 0xFFFF);
  tft.drawCircle(ccx, ccy, 9, 0xFFFF);
  tft.drawCircle(ccx, ccy, 8, 0xFFFF);

  // Flash dot (top-right of IG square)
  tft.fillCircle(ix + iconSz - 10, iy + 10, 2, 0xFFFF);

  // Redraw split line on top of logo
  int16_t splitY = LOGO_PANEL_H / 2 - LP_SPLIT_H / 2;
  tft.fillRect(x, y + splitY, LOGO_PANEL_W, LP_SPLIT_H, LP_SPLIT_COLOR);
}

#endif // LOGOS_H
