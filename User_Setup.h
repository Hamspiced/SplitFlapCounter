// -----------------------------------------------------------------
//  TFT_eSPI  User_Setup.h  –  ESP32-2432S028R  "Cheap Yellow Display"
//
//  Copy this file into your Arduino/libraries/TFT_eSPI/ folder,
//  replacing the existing User_Setup.h, then compile the sketch.
// -----------------------------------------------------------------

#define USER_SETUP_INFO "CYD_ESP32_2432S028R"

// ---- Driver ----
#define ILI9341_2_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---- SPI pins (HSPI) ----
#define TFT_MISO  12
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1   // RST tied to board reset

// ---- Backlight ----
#define TFT_BL    21
#define TFT_BACKLIGHT_ON HIGH

// ---- Fonts ----
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ---- SPI speed ----
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
