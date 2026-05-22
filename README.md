# SplitFlap Follower Counter

A split-flap style YouTube subscriber and Instagram follower counter for the **ESP32 Cheap Yellow Display** (ESP32-2432S028R).

The 2.8" TFT shows your follower counts with a mechanical split-flap animation — dark panels with amber text that flip through digits just like an airport departure board. It alternates between YouTube and Instagram, displaying the platform logo alongside the count.

All settings (WiFi credentials, API keys, display brightness, animation speed, and timing) are configurable through a built-in web interface.

---

## Hardware

- **ESP32-2432S028R** "Cheap Yellow Display" (CYD)
  - ESP32-WROOM-32, 2.8" ILI9341 TFT (240×320), resistive touch
  - [AliExpress](https://www.aliexpress.com/item/1005004502250619.html)

No additional wiring or components needed — the CYD is an all-in-one board.

---

## Libraries Required

Install these from the Arduino Library Manager:

| Library | Author | Purpose |
|---------|--------|---------|
| `TFT_eSPI` | Bodmer | TFT display driver |
| `ArduinoJson` | Benoît Blanchon | JSON parsing for API responses |

The following are included with the ESP32 Arduino core:
- `WiFi`, `WiFiClientSecure`, `HTTPClient`
- `WebServer`
- `Preferences`
- `SPI`

---

## Setup

### 1. Install ESP32 Board Support

In Arduino IDE: **File → Preferences → Additional Boards Manager URLs**, add:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
Then install **"ESP32 by Espressif Systems"** from the Boards Manager.

### 2. Configure TFT_eSPI

Copy the included `User_Setup.h` into your TFT_eSPI library folder, replacing the existing file:

- **macOS**: `~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h`
- **Windows**: `C:\Users\<YOU>\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h`

This configures the correct ILI9341 driver and CYD pin mapping.

### 3. Select Board

In Arduino IDE:
- **Board**: ESP32 Dev Module
- **Upload Speed**: 921600
- **Flash Size**: 4MB

### 4. Upload

Open `SplitFlapCounter.ino` and upload to your CYD.

### 5. Configure via Web Interface

1. On first boot, edit the default WiFi credentials in the sketch, or connect to the serial console to find the IP address.
2. Navigate to the device's IP address in a web browser.
3. Enter your YouTube API key, Channel ID, Instagram token, and User ID.
4. Adjust display brightness, flip animation speed, and switching interval.
5. Click **Save Settings**.

---

## API Setup

### YouTube

1. Go to the [Google Cloud Console](https://console.cloud.google.com/apis/credentials)
2. Create a project and enable the **YouTube Data API v3**
3. Create an API key
4. Find your Channel ID at [youtube.com/account_advanced](https://www.youtube.com/account_advanced)

### Instagram

1. Create a [Meta Developer App](https://developers.facebook.com/)
2. Add the Instagram product and configure business login
3. Generate an access token (valid for 60 days)
4. Find your User ID via the Graph API Explorer

The web interface includes a token refresh button and automatic renewal at 45 days.

Full instructions are the same as the [8x32 WOPR Display project](https://github.com/your-repo/8x32_WOPR_Display_With_IG_and_YT_Follower_Counter).

---

## Web Interface Features

- WiFi configuration
- YouTube and Instagram API key management
- API key testing (test buttons verify keys without saving)
- Instagram token refresh/extend with auto-renewal at 45 days
- Display brightness (0-100%)
- Flip animation speed
- Platform switch interval
- Data fetch interval
- Enable/disable Instagram display
- Enable/disable channel/account name row
- Enable/disable splitflap click sound
- Custom display lines with per-line hold timers (up to 10)
- Manual platform switch trigger
- Reboot button

---

## How It Works

1. Connects to WiFi (falls back to AP mode for 2 minutes if saved network unavailable)
2. Fetches YouTube subscriber count and channel name, Instagram follower count and username
3. Displays the YouTube channel name (top row, all flaps simultaneously) and subscriber count (bottom row, sequential right-to-left) with splitflap animation and mechanical click sound
4. After a configurable interval, flips to Instagram follower count with username
5. Custom text lines display after follower counts, each with independent hold timers
6. Repeats, periodically re-fetching fresh data
7. Web server runs continuously for configuration changes

---

## Customization

- **Colors**: Edit `SF_TEXT`, `SF_PANEL`, `SF_BG` in `splitflap.h`
- **Cell size**: Adjust cell dimensions via `begin()` parameters in the .ino
- **Animation speed**: Change `SF_ANIM_FRAMES` in `splitflap.h` or Flip Speed in the web UI
- **Logos**: Modify the procedural drawing functions in `logos.h`
- **Sound**: Toggle splitflap click sound on/off in the web UI
- **Channel names**: Toggle the name row display on/off in the web UI
- **Custom messages**: Add up to 10 custom text lines with individual hold timers via the web UI

---

## License

MIT License

Designed by Midwest Gadgets
