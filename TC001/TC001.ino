#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include <FastLED.h>

// =====================================================================
//  Hardware — Ulanzi TC001 (ESP32-WROOM-32D + 8x32 WS2812B matrix)
// =====================================================================
#define LED_PIN       32
#define NUM_LEDS      256
#define MATRIX_W      32
#define MATRIX_H      8

#define BUZZER_PIN    15
#define BTN_LEFT      26
#define BTN_MID       27
#define BTN_RIGHT     14
#define LDR_PIN       35
#define BATTERY_PIN   34

CRGB leds[NUM_LEDS];

// Serpentine pixel mapping (even rows L→R, odd rows R→L)
inline void setPixel(int x, int y, CRGB color) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return;
  int idx = (y & 1) ? (y * MATRIX_W + (MATRIX_W - 1 - x)) : (y * MATRIX_W + x);
  leds[idx] = color;
}

// Now include display modules (they use setPixel)
#include "font3x5.h"
#include "MatrixLogos.h"
#include "SplitFlapMatrix.h"

// =====================================================================
//  Layout — 8px icon | 1px gap | 6 digit cells (23px)
// =====================================================================
#define ICON_X       0
#define ICON_Y       0
#define FLAP_START_X 9
#define FLAP_CELLS   6

SplitFlapMatrix flap;

// =====================================================================
//  User Settings
// =====================================================================
uint8_t  brightness      = 40;    // LED brightness 0..100 (mapped to 0..255)
uint16_t flipSpeedMs     = 40;    // per-frame delay for splitflap
uint32_t displayTimeSec  = 10;    // seconds each platform is shown
uint32_t fetchPeriodSec  = 3600;  // fetch interval
bool     enableSound     = true;
bool     autoBrightness  = false;

// Colors (stored as 0xRRGGBB)
uint32_t colorDigit = 0xFFB400;   // digit/text color (amber)
uint32_t colorFlap  = 0x0C0A02;   // flap panel color (dark amber)
uint32_t colorBg    = 0x000000;   // background color (black)

// Credentials / API
String WIFI_SSID   = "EnterSSIDHere";
String WIFI_PASS   = "EnterPSKHere";
String YT_API_KEY  = "EnterYoutubeAPIHere";
String YT_CHAN_ID  = "EnterYouTubeChannelIDHere";
bool   ENABLE_IG   = true;
String IG_TOKEN    = "EnterInstagramAPITokenHere";
String IG_USER_ID  = "EnterInstagramUserIDHere";
String IG_SECRET   = "EnterInstagramAppSecretHere";
uint32_t tokenRefreshEpoch = 0;

WiFiClientSecure secureClient;
WebServer server(80);
Preferences prefs;
volatile bool triggerSwitch = false;

// AP fallback
#define AP_SSID    "SplitFlap-Setup"
#define AP_TIMEOUT 120000
DNSServer dnsServer;
bool apModeActive = false;

// Custom display messages
#define MAX_CUSTOM_MSGS 10
#define MSG_MAX_LEN     6
String   customMsgs[MAX_CUSTOM_MSGS];
uint16_t customMsgHoldSec[MAX_CUSTOM_MSGS];
uint8_t  customMsgCount = 0;

// Forward declarations
void saveToPrefs();

// Color helpers
String colorToHex(uint32_t c) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c);
  return String(buf);
}

uint32_t hexToColor(const String &s) {
  if (s.length() < 7 || s[0] != '#') return 0;
  return strtoul(s.c_str() + 1, NULL, 16);
}

void applyColors() {
  CRGB tc = CRGB((colorDigit >> 16) & 0xFF, (colorDigit >> 8) & 0xFF, colorDigit & 0xFF);
  CRGB dt = tc; dt.nscale8(80);
  CRGB pt = CRGB((colorFlap >> 16) & 0xFF, (colorFlap >> 8) & 0xFF, colorFlap & 0xFF);
  CRGB pb = pt; pb.nscale8(160);
  CRGB bg = CRGB((colorBg >> 16) & 0xFF, (colorBg >> 8) & 0xFF, colorBg & 0xFF);
  flap.textColor    = tc;
  flap.dimTextColor = dt;
  flap.panelTop     = pt;
  flap.panelBot     = pb;
  flap.splitColor   = bg;
  flap.gapColor     = bg;
}

// State
long ytSubs       = -1;
long igFollowers  = -1;
String ytChannelName = "";
String igUsername    = "";
uint32_t lastFetch = 0;
uint8_t  slideIndex = 0;
bool     slideAnimDone = false;
uint32_t slideHoldStart = 0;

// Button debounce
uint32_t lastBtnPress = 0;
#define BTN_DEBOUNCE 200

// =====================================================================
//  Helpers
// =====================================================================
String fmtCountCompact(long n) {
  if (n < 0) return "err";
  String raw = String(n);
  String result = "";
  int len = raw.length();
  for (int i = 0; i < len; i++) {
    if (i > 0 && (len - i) % 3 == 0) result += ',';
    result += raw[i];
  }
  if ((int)result.length() > FLAP_CELLS) result = result.substring(0, FLAP_CELLS);
  return result;
}

bool httpsGET(const String &url, String &payload) {
  HTTPClient http;
  http.begin(secureClient, url);
  int code = http.GET();
  if (code == HTTP_CODE_OK) { payload = http.getString(); http.end(); return true; }
  Serial.println("[HTTP] Request failed - code: " + String(code));
  payload = http.getString();
  http.end();
  return false;
}

// =====================================================================
//  API Fetchers
// =====================================================================
long fetchYouTubeSubs() {
  Serial.println("[YT] Fetching subscriber count...");
  String url = String("https://www.googleapis.com/youtube/v3/channels?part=statistics,snippet&id=") +
               YT_CHAN_ID + "&key=" + YT_API_KEY;
  String body;
  if (!httpsGET(url, body)) { return -1; }
  DynamicJsonDocument doc(6144);
  if (deserializeJson(doc, body)) return -1;
  if (!doc["items"][0]["snippet"]["title"].isNull()) {
    ytChannelName = doc["items"][0]["snippet"]["title"].as<String>();
    ytChannelName.toUpperCase();
  }
  if (!doc["items"][0]["statistics"]["subscriberCount"].isNull()) {
    long count = String(doc["items"][0]["statistics"]["subscriberCount"].as<const char*>()).toInt();
    Serial.println("[YT] Subscribers: " + String(count));
    return count;
  }
  return -1;
}

long fetchInstagramFollowers() {
  if (!ENABLE_IG) return -1;
  Serial.println("[IG] Fetching follower count...");
  String url = String("https://graph.instagram.com/v19.0/") + IG_USER_ID +
               "?fields=followers_count,username&access_token=" + IG_TOKEN;
  String body;
  if (!httpsGET(url, body)) { return -1; }
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, body)) return -1;
  if (!doc["username"].isNull()) {
    igUsername = doc["username"].as<String>();
    igUsername.toUpperCase();
  }
  if (!doc["followers_count"].isNull()) {
    long count = doc["followers_count"].as<long>();
    Serial.println("[IG] Followers: " + String(count));
    return count;
  }
  return -1;
}

void checkAutoRefreshToken() {
  if (!ENABLE_IG || IG_TOKEN.length() == 0 || tokenRefreshEpoch == 0) return;
  time_t now = time(nullptr);
  if (now < 1000000000) return;
  uint32_t ageDays = ((uint32_t)now - tokenRefreshEpoch) / 86400;
  if (ageDays >= 45) {
    String url = "https://graph.instagram.com/refresh_access_token?grant_type=ig_refresh_token&access_token=" + IG_TOKEN;
    String body;
    if (httpsGET(url, body)) {
      DynamicJsonDocument doc(2048);
      if (!deserializeJson(doc, body) && !doc["access_token"].isNull()) {
        IG_TOKEN = doc["access_token"].as<String>();
        tokenRefreshEpoch = (uint32_t)now;
        saveToPrefs();
        Serial.println("[IG] Token auto-refreshed");
      }
    }
  }
}

// =====================================================================
//  Display helpers
// =====================================================================
void flapClick() {
  if (!enableSound) return;
  tone(BUZZER_PIN, 3000, 3);
}

void setBrightness(uint8_t pct) {
  uint8_t val = map(constrain(pct, 0, 100), 0, 100, 0, 255);
  FastLED.setBrightness(val);
}

void drawLogoArea(bool youtube) {
  CRGB bg = CRGB((colorBg >> 16) & 0xFF, (colorBg >> 8) & 0xFF, colorBg & 0xFF);
  if (youtube) {
    drawIcon(setPixel, YT_ICON, ICON_X, ICON_Y, CRGB(220, 0, 0), CRGB::White, bg);
  } else {
    drawIcon(setPixel, IG_ICON, ICON_X, ICON_Y, CRGB(193, 53, 132), CRGB::White, bg);
  }
  // Fill gap column between icon and digits
  for (int y = 0; y < MATRIX_H; y++)
    setPixel(ICON_SIZE, y, bg);
}

// =====================================================================
//  NVS Persistent Storage
// =====================================================================
void saveToPrefs() {
  prefs.putString("yt_api",  YT_API_KEY);
  prefs.putString("yt_chan", YT_CHAN_ID);
  prefs.putString("ig_tok",  IG_TOKEN);
  prefs.putString("ig_uid",  IG_USER_ID);
  prefs.putString("ig_sec",  IG_SECRET);
  prefs.putUInt("tokEpoch",  tokenRefreshEpoch);
  prefs.putUChar("bright",   brightness);
  prefs.putUShort("flipMs",  flipSpeedMs);
  prefs.putUInt("dispSec",   displayTimeSec);
  prefs.putUInt("fetchSec",  fetchPeriodSec);
  prefs.putBool("ig_en",     ENABLE_IG);
  prefs.putBool("snd_en",    enableSound);
  prefs.putUInt("cDigit",    colorDigit);
  prefs.putUInt("cFlap",     colorFlap);
  prefs.putUInt("cBg",       colorBg);
  prefs.putString("ssid",    WIFI_SSID);
  prefs.putString("pass",    WIFI_PASS);
  prefs.putUChar("msgCnt",   customMsgCount);
  for (uint8_t i = 0; i < MAX_CUSTOM_MSGS; i++) {
    String key = "msg" + String(i);
    String tkey = "mst" + String(i);
    if (i < customMsgCount) {
      prefs.putString(key.c_str(), customMsgs[i]);
      prefs.putUShort(tkey.c_str(), customMsgHoldSec[i]);
    } else {
      prefs.remove(key.c_str());
      prefs.remove(tkey.c_str());
    }
  }
}

void loadFromPrefs() {
  YT_API_KEY   = prefs.getString("yt_api",  YT_API_KEY);
  YT_CHAN_ID   = prefs.getString("yt_chan",  YT_CHAN_ID);
  IG_TOKEN     = prefs.getString("ig_tok",  IG_TOKEN);
  IG_USER_ID   = prefs.getString("ig_uid",  IG_USER_ID);
  IG_SECRET    = prefs.getString("ig_sec",  IG_SECRET);
  tokenRefreshEpoch = prefs.getUInt("tokEpoch", tokenRefreshEpoch);
  brightness   = prefs.getUChar("bright",  brightness);
  flipSpeedMs  = prefs.getUShort("flipMs", flipSpeedMs);
  displayTimeSec = prefs.getUInt("dispSec", displayTimeSec);
  fetchPeriodSec = prefs.getUInt("fetchSec", fetchPeriodSec);
  ENABLE_IG    = prefs.getBool("ig_en",    ENABLE_IG);
  enableSound  = prefs.getBool("snd_en",   enableSound);
  colorDigit   = prefs.getUInt("cDigit",   colorDigit);
  colorFlap    = prefs.getUInt("cFlap",    colorFlap);
  colorBg      = prefs.getUInt("cBg",      colorBg);
  WIFI_SSID    = prefs.getString("ssid",   WIFI_SSID);
  WIFI_PASS    = prefs.getString("pass",   WIFI_PASS);
  customMsgCount = prefs.getUChar("msgCnt", 0);
  if (customMsgCount > MAX_CUSTOM_MSGS) customMsgCount = MAX_CUSTOM_MSGS;
  for (uint8_t i = 0; i < customMsgCount; i++) {
    customMsgs[i] = prefs.getString(("msg" + String(i)).c_str(), "");
    customMsgs[i] = customMsgs[i].substring(0, MSG_MAX_LEN);
    customMsgHoldSec[i] = prefs.getUShort(("mst" + String(i)).c_str(), 5);
  }
}

// =====================================================================
//  Web UI
// =====================================================================
String htmlPage() {
  String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "0.0.0.0";
  String s;
  s.reserve(9000);
  s += "<!doctype html><html><head><meta charset='utf-8'/><meta name='viewport' content='width=device-width,initial-scale=1'/>";
  s += "<title>SplitFlap Counter TC001</title>";
  s += "<style>*{box-sizing:border-box}body{font-family:sans-serif;background:#0c141b;color:#eaf6fb;padding:20px}";
  s += ".container{max-width:680px;margin:0 auto}";
  s += "h1{font-size:20px}label{display:block;margin-top:10px;font-weight:700}";
  s += "input{width:100%;padding:10px;border-radius:8px;border:1px solid #2a3b48;background:#0f1a22;color:#eaf6fb}";
  s += ".row{display:grid;grid-template-columns:1fr 1fr;gap:10px}";
  s += "button{margin-top:16px;padding:10px 14px;border-radius:10px;border:0;background:#00e4ff;color:#06222d;font-weight:800;cursor:pointer}";
  s += ".muted{opacity:.8;font-size:.9rem;margin-top:4px}";
  s += ".box{border:1px solid #244050;border-radius:12px;padding:14px;margin-top:16px}";
  s += ".result{margin:8px 0;padding:8px 12px;border-radius:6px;font-size:.9rem}";
  s += ".test-btn{margin-top:8px;padding:8px 14px;border-radius:10px;border:1px solid #00e4ff;background:transparent;color:#00e4ff;cursor:pointer;font-weight:700}";
  s += "a{color:#00e4ff}a:visited{color:#00c4dd}";
  s += ".footer{text-align:center;margin-top:30px;padding:20px 0;border-top:1px solid #244050;opacity:.7}";
  s += ".footer-text{font-size:.8rem}";
  s += "</style></head><body><div class='container'>";
  s += "<h1>SplitFlap Counter &mdash; TC001</h1>";
  s += "<div class='muted'>Device IP: " + ip + "</div>";

  // WiFi
  s += "<form method='POST' action='/save'><div class='box'><h3>WiFi</h3>";
  s += "<label>SSID</label><input name='ssid' value='" + WIFI_SSID + "'/>";
  s += "<label>Password</label><input name='wpass' type='password' value='" + WIFI_PASS + "'/>";
  s += "</div>";

  // APIs
  s += "<div class='box'><h3>APIs</h3>";
  s += "<label>YouTube API Key</label><input name='yt_api' value='" + YT_API_KEY + "'/>";
  s += "<label>YouTube Channel ID</label><input name='yt_chan' value='" + YT_CHAN_ID + "'/>";
  s += "<label>Instagram Access Token</label><input name='ig_token' value='" + IG_TOKEN + "'/>";
  s += "<label>Instagram User ID</label><input name='ig_uid' value='" + IG_USER_ID + "'/>";
  s += "<label>Instagram App Secret</label><input name='ig_secret' value='" + IG_SECRET + "'/>";
  s += "</div>";

  // Display
  s += "<div class='box'><h3>Display & Timing</h3><div class='row'>";
  s += "<div><label>Brightness (0-100%)</label><input name='bright' type='number' min='0' max='100' value='" + String(brightness) + "'/></div>";
  s += "<div><label>Flip Speed (ms/frame)</label><input name='flipms' type='number' min='5' max='200' value='" + String(flipSpeedMs) + "'/></div>";
  s += "</div><div class='row'>";
  s += "<div><label>Switch Interval (sec)</label><input name='dispsec' type='number' min='3' value='" + String(displayTimeSec) + "'/></div>";
  s += "<div><label>Fetch Period (sec)</label><input name='fetchsec' type='number' min='60' value='" + String(fetchPeriodSec) + "'/></div>";
  s += "</div><div class='row'>";
  s += "<div><label>Enable Instagram (0/1)</label><input name='ig_en' type='number' min='0' max='1' value='" + String(ENABLE_IG ? 1 : 0) + "'/></div>";
  s += "<div><label>Click Sound (0/1)</label><input name='snd_en' type='number' min='0' max='1' value='" + String(enableSound ? 1 : 0) + "'/></div>";
  s += "</div>";
  s += "<h4 style='margin-top:14px'>Colors</h4><div class='row' style='grid-template-columns:1fr 1fr 1fr'>";
  s += "<div><label>Digit Color</label><input name='c_digit' type='color' value='" + colorToHex(colorDigit) + "' style='height:44px;padding:2px'/></div>";
  s += "<div><label>Flap Color</label><input name='c_flap' type='color' value='" + colorToHex(colorFlap) + "' style='height:44px;padding:2px'/></div>";
  s += "<div><label>Background</label><input name='c_bg' type='color' value='" + colorToHex(colorBg) + "' style='height:44px;padding:2px'/></div>";
  s += "</div></div>";

  // Custom Messages
  s += "<div class='box'><h3>Custom Display Lines</h3>";
  s += "<div class='muted'>Add custom text that displays after follower counts. Max " + String(MSG_MAX_LEN) + " characters per line.</div>";
  s += "<div id='msglist'>";
  for (uint8_t i = 0; i < customMsgCount; i++) {
    s += "<div style='display:flex;gap:8px;margin-top:8px;align-items:center'>";
    s += "<input name='cmsg" + String(i) + "' maxlength='" + String(MSG_MAX_LEN) + "' value='" + customMsgs[i] + "' style='flex:1;text-transform:uppercase;font-family:monospace;letter-spacing:2px'/>";
    s += "<input name='csec" + String(i) + "' type='number' min='1' max='999' value='" + String(customMsgHoldSec[i]) + "' style='width:70px' title='Hold seconds'/>";
    s += "<span class='muted' style='white-space:nowrap'>sec</span>";
    s += "<button type='button' onclick='this.parentElement.remove()' style='margin:0;padding:6px 12px;background:#f44;color:#fff;font-size:.8rem;border-radius:8px'>X</button></div>";
  }
  s += "</div>";
  s += "<button type='button' class='test-btn' id='addMsgBtn' style='margin-top:8px'>+ Add Line</button>";
  s += "<input type='hidden' name='msg_count' id='msg_count' value='" + String(customMsgCount) + "'/>";
  s += "</div>";

  s += "<button type='submit'>Save Settings</button></form>";

  // Test
  s += "<div class='box'><h3>Test API Keys</h3>";
  s += "<button type='button' class='test-btn' onclick='testYT()'>Test YouTube</button>";
  s += "<div id='yt_res' class='result'></div>";
  s += "<button type='button' class='test-btn' onclick='testIG()'>Test Instagram</button>";
  s += "<div id='ig_res' class='result'></div></div>";

  // IG Token
  s += "<div class='box'><h3>Instagram Token Management</h3>";
  String tokenAge = "Unknown";
  time_t tNow = time(nullptr);
  if (tokenRefreshEpoch > 0 && tNow > 1000000000) {
    uint32_t days = ((uint32_t)tNow - tokenRefreshEpoch) / 86400;
    uint32_t remaining = (days < 60) ? (60 - days) : 0;
    tokenAge = String(days) + " days old - " + String(remaining) + " days remaining";
  }
  s += "<p class='muted'>Token status: " + tokenAge + "</p>";
  s += "<button type='button' class='test-btn' onclick='refreshIG()'>Refresh / Extend IG Token</button>";
  s += "<div id='ref_res' class='result'></div></div>";

  // Control
  s += "<div class='box'><h3>Display Control</h3>";
  s += "<button type='button' class='test-btn' onclick='doTrigger()'>Switch Platform Now</button>";
  s += "<div id='trig_res' class='result'></div></div>";

  s += "<form method='POST' action='/reboot'><button>Reboot</button></form>";
  s += "<div class='muted'>Changes apply immediately; some may require a reboot.</div>";

  s += "<div class='footer'><div class='footer-text'>SplitFlap Counter TC001 v1.0 &mdash; Designed by Midwest Gadgets</div></div>";

  // JavaScript
  s += "<script>";
  s += "function testYT(){var el=document.getElementById('yt_res');el.style.color='#aaa';el.innerText='Testing...';"
       "var a=document.querySelector('[name=yt_api]').value;var c=document.querySelector('[name=yt_chan]').value;"
       "fetch('/test_yt?api='+encodeURIComponent(a)+'&chan='+encodeURIComponent(c))"
       ".then(r=>r.json()).then(d=>{"
       "if(d.ok){el.style.color='#0f0';el.innerText='OK - Subscribers: '+d.subs;}"
       "else{el.style.color='#f44';el.innerText='Failed - '+d.error;}"
       "}).catch(e=>{el.style.color='#f44';el.innerText='Error: '+e;});}";
  s += "function testIG(){var el=document.getElementById('ig_res');el.style.color='#aaa';el.innerText='Testing...';"
       "var t=document.querySelector('[name=ig_token]').value;var u=document.querySelector('[name=ig_uid]').value;"
       "fetch('/test_ig?token='+encodeURIComponent(t)+'&uid='+encodeURIComponent(u))"
       ".then(r=>r.json()).then(d=>{"
       "if(d.ok){el.style.color='#0f0';el.innerText='OK - Followers: '+d.followers;}"
       "else{el.style.color='#f44';el.innerText='Failed - '+d.error;}"
       "}).catch(e=>{el.style.color='#f44';el.innerText='Error: '+e;});}";
  s += "function refreshIG(){var el=document.getElementById('ref_res');el.style.color='#aaa';el.innerText='Refreshing...';"
       "var t=document.querySelector('[name=ig_token]').value;var s=document.querySelector('[name=ig_secret]').value;"
       "fetch('/refresh_ig?token='+encodeURIComponent(t)+'&secret='+encodeURIComponent(s))"
       ".then(r=>r.json()).then(d=>{"
       "if(d.ok){el.style.color='#0f0';el.innerText='Token refreshed! Valid for '+d.expires_days+' days.';"
       "document.querySelector('[name=ig_token]').value=d.token;}"
       "else{el.style.color='#f44';el.innerText='Failed - '+d.error;}"
       "}).catch(e=>{el.style.color='#f44';el.innerText='Error: '+e;});}";
  s += "function doTrigger(){var el=document.getElementById('trig_res');el.style.color='#aaa';el.innerText='Switching...';"
       "fetch('/trigger').then(r=>r.json()).then(d=>{"
       "if(d.ok){el.style.color='#0f0';el.innerText='Switched!';}"
       "}).catch(e=>{el.style.color='#f44';el.innerText='Error: '+e;});}";
  s += "var mi=" + String(customMsgCount) + ";";
  s += "document.getElementById('addMsgBtn').onclick=function(){"
       "if(document.querySelectorAll('#msglist>div').length>=10)return;"
       "var d=document.createElement('div');"
       "d.style='display:flex;gap:8px;margin-top:8px;align-items:center';"
       "d.innerHTML='<input name=cmsg'+mi+' maxlength=" + String(MSG_MAX_LEN) + " placeholder=TEXT style=flex:1;text-transform:uppercase;font-family:monospace;letter-spacing:2px />'"
       "+'<input name=csec'+mi+' type=number min=1 max=999 value=5 style=width:70px title=Hold+seconds />'"
       "+'<span class=muted style=white-space:nowrap>sec</span>'"
       "+'<button type=button onclick=this.parentElement.remove() style=margin:0;padding:6px+12px;background:#f44;color:#fff;font-size:.8rem;border-radius:8px>X</button>';"
       "document.getElementById('msglist').appendChild(d);mi++;};";
  s += "document.querySelector('form').onsubmit=function(){"
       "var rows=document.querySelectorAll('#msglist>div');"
       "for(var i=0;i<rows.length;i++){rows[i].querySelectorAll('input')[0].name='cmsg'+i;rows[i].querySelectorAll('input')[1].name='csec'+i;}"
       "document.getElementById('msg_count').value=rows.length;};";
  s += "</script></div></body></html>";
  return s;
}

// =====================================================================
//  HTTP Handlers
// =====================================================================
void handleRoot()   { server.send(200, "text/html", htmlPage()); }
void handleReboot() { server.send(200, "text/plain", "Rebooting..."); delay(300); ESP.restart(); }

void handleSave() {
  if (server.hasArg("ssid"))     WIFI_SSID = server.arg("ssid");
  if (server.hasArg("wpass"))    WIFI_PASS = server.arg("wpass");
  if (server.hasArg("yt_api"))   YT_API_KEY = server.arg("yt_api");
  if (server.hasArg("yt_chan"))   YT_CHAN_ID = server.arg("yt_chan");
  if (server.hasArg("ig_token")) {
    String newTok = server.arg("ig_token");
    if (newTok != IG_TOKEN) {
      IG_TOKEN = newTok;
      time_t now = time(nullptr);
      if (now > 1000000000) tokenRefreshEpoch = (uint32_t)now;
    }
  }
  if (server.hasArg("ig_uid"))    IG_USER_ID = server.arg("ig_uid");
  if (server.hasArg("ig_secret")) IG_SECRET  = server.arg("ig_secret");
  if (server.hasArg("bright"))    { brightness = constrain(server.arg("bright").toInt(), 0, 100); setBrightness(brightness); }
  if (server.hasArg("flipms"))    flipSpeedMs = constrain(server.arg("flipms").toInt(), 5, 200);
  if (server.hasArg("dispsec"))   displayTimeSec = (uint32_t)server.arg("dispsec").toInt();
  if (server.hasArg("fetchsec"))  fetchPeriodSec = (uint32_t)server.arg("fetchsec").toInt();
  if (server.hasArg("ig_en"))     ENABLE_IG = (server.arg("ig_en").toInt() != 0);
  if (server.hasArg("snd_en"))    enableSound = (server.arg("snd_en").toInt() != 0);
  if (server.hasArg("c_digit"))   { colorDigit = hexToColor(server.arg("c_digit")); }
  if (server.hasArg("c_flap"))    { colorFlap  = hexToColor(server.arg("c_flap")); }
  if (server.hasArg("c_bg"))      { colorBg    = hexToColor(server.arg("c_bg")); }
  applyColors();
  flap.drawAll();
  if (server.hasArg("msg_count")) {
    uint8_t cnt = constrain(server.arg("msg_count").toInt(), 0, MAX_CUSTOM_MSGS);
    customMsgCount = 0;
    for (uint8_t i = 0; i < cnt; i++) {
      String key = "cmsg" + String(i);
      if (server.hasArg(key)) {
        String val = server.arg(key);
        val.trim(); val.toUpperCase();
        if (val.length() > 0) {
          customMsgs[customMsgCount] = val.substring(0, MSG_MAX_LEN);
          String skey = "csec" + String(i);
          customMsgHoldSec[customMsgCount] = server.hasArg(skey) ? constrain(server.arg(skey).toInt(), 1, 999) : 5;
          customMsgCount++;
        }
      }
    }
  }
  saveToPrefs();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "Saved");
}

void handleTestYT() {
  String api  = server.hasArg("api")  ? server.arg("api")  : YT_API_KEY;
  String chan  = server.hasArg("chan") ? server.arg("chan") : YT_CHAN_ID;
  String url  = String("https://www.googleapis.com/youtube/v3/channels?part=statistics&id=") + chan + "&key=" + api;
  String body;
  DynamicJsonDocument resp(512);
  if (httpsGET(url, body)) {
    DynamicJsonDocument doc(4096);
    if (!deserializeJson(doc, body) && !doc["items"][0]["statistics"]["subscriberCount"].isNull()) {
      resp["ok"] = true;
      resp["subs"] = String(doc["items"][0]["statistics"]["subscriberCount"].as<const char*>()).toInt();
    } else { resp["ok"] = false; resp["error"] = "No subscriber data"; }
  } else {
    resp["ok"] = false; resp["error"] = "API request failed";
  }
  String out; serializeJson(resp, out);
  server.send(200, "application/json", out);
}

void handleTestIG() {
  String token = server.hasArg("token") ? server.arg("token") : IG_TOKEN;
  String uid   = server.hasArg("uid")   ? server.arg("uid")   : IG_USER_ID;
  String url   = String("https://graph.instagram.com/v19.0/") + uid + "?fields=followers_count&access_token=" + token;
  String body;
  DynamicJsonDocument resp(512);
  if (httpsGET(url, body)) {
    DynamicJsonDocument doc(4096);
    if (!deserializeJson(doc, body) && !doc["followers_count"].isNull()) {
      resp["ok"] = true; resp["followers"] = doc["followers_count"].as<long>();
    } else { resp["ok"] = false; resp["error"] = "No follower data"; }
  } else { resp["ok"] = false; resp["error"] = "API request failed"; }
  String out; serializeJson(resp, out);
  server.send(200, "application/json", out);
}

void handleRefreshIG() {
  String token  = server.hasArg("token")  ? server.arg("token")  : IG_TOKEN;
  String secret = server.hasArg("secret") ? server.arg("secret") : IG_SECRET;
  String url;
  if (secret.length() > 0)
    url = "https://graph.instagram.com/access_token?grant_type=ig_exchange_token&client_secret=" + secret + "&access_token=" + token;
  else
    url = "https://graph.instagram.com/refresh_access_token?grant_type=ig_refresh_token&access_token=" + token;
  String body;
  DynamicJsonDocument resp(1024);
  if (httpsGET(url, body)) {
    DynamicJsonDocument doc(2048);
    if (!deserializeJson(doc, body) && !doc["access_token"].isNull()) {
      String newToken = doc["access_token"].as<String>();
      long expiresIn  = doc["expires_in"] | 0;
      IG_TOKEN = newToken;
      time_t now = time(nullptr);
      if (now > 1000000000) tokenRefreshEpoch = (uint32_t)now;
      saveToPrefs();
      resp["ok"] = true; resp["token"] = newToken; resp["expires_days"] = expiresIn / 86400;
    } else { resp["ok"] = false; resp["error"] = "Unexpected response"; }
  } else { resp["ok"] = false; resp["error"] = "Token refresh failed"; }
  String out; serializeJson(resp, out);
  server.send(200, "application/json", out);
}

void handleTrigger() {
  triggerSwitch = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

// =====================================================================
//  Button handling
// =====================================================================
void pollButtons() {
  uint32_t now = millis();
  if (now - lastBtnPress < BTN_DEBOUNCE) return;

  if (digitalRead(BTN_LEFT) == LOW) {
    lastBtnPress = now;
    if (brightness >= 10) brightness -= 10; else brightness = 0;
    setBrightness(brightness);
    saveToPrefs();
  }
  if (digitalRead(BTN_MID) == LOW) {
    lastBtnPress = now;
    triggerSwitch = true;
  }
  if (digitalRead(BTN_RIGHT) == LOW) {
    lastBtnPress = now;
    if (brightness <= 90) brightness += 10; else brightness = 100;
    setBrightness(brightness);
    saveToPrefs();
  }
}

// =====================================================================
//  Web server route registration
// =====================================================================
void registerRoutes() {
  server.on("/",          HTTP_GET,  handleRoot);
  server.on("/save",      HTTP_POST, handleSave);
  server.on("/reboot",    HTTP_POST, handleReboot);
  server.on("/test_yt",   HTTP_GET,  handleTestYT);
  server.on("/test_ig",   HTTP_GET,  handleTestIG);
  server.on("/trigger",   HTTP_GET,  handleTrigger);
  server.on("/refresh_ig", HTTP_GET, handleRefreshIG);
}

// =====================================================================
//  Setup
// =====================================================================
void setup() {
  // Silence the buzzer whine (per awtrix3 — must be done early)
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  delay(1000);
  Serial.begin(115200);
  Serial.println("\n[BOOT] SplitFlap Counter TC001 starting...");

  // Buttons
  pinMode(BTN_LEFT,  INPUT_PULLUP);
  pinMode(BTN_MID,   INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  // NVS
  prefs.begin("sflap", false);
  loadFromPrefs();

  // LED matrix
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 750);
  setBrightness(brightness);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // SplitFlap
  flap.begin(FLAP_START_X, FLAP_CELLS);
  applyColors();
  flap.drawAll();
  FastLED.show();

  // WiFi — try saved network first
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("splitflap-tc001");
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000UL) { delay(250); }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] STA failed - starting AP: " AP_SSID);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    delay(100);
    IPAddress apIP = WiFi.softAPIP();
    dnsServer.start(53, "*", apIP);
    apModeActive = true;
    Serial.println("[WiFi] AP IP: " + apIP.toString());

    registerRoutes();
    server.begin();

    uint32_t apStart = millis();
    while (millis() - apStart < AP_TIMEOUT) {
      dnsServer.processNextRequest();
      server.handleClient();
      delay(10);
    }

    Serial.println("[WiFi] AP timeout - switching to STA");
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 10000UL) { delay(250); }
    apModeActive = false;
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("[WiFi] Connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("[WiFi] Running offline");
  }

  secureClient.setInsecure();
  randomSeed(esp_random());

  if (!apModeActive) {
    registerRoutes();
    server.begin();
  }

  // Initial fetch
  if (WiFi.status() == WL_CONNECTED) {
    checkAutoRefreshToken();
    ytSubs      = fetchYouTubeSubs();
    igFollowers = ENABLE_IG ? fetchInstagramFollowers() : -1;
    lastFetch   = millis();

    // Show YouTube first
    slideIndex = 0;
    drawLogoArea(true);
    flap.setNumericMode(true);
    flap.setTarget(fmtCountCompact(ytSubs).c_str());
    flap.drawAll();
    FastLED.show();
  }
}

// =====================================================================
//  Slide Rotation:  YT → [IG] → custom1 → custom2 → ... → back to YT
// =====================================================================
void advanceSlide() {
  uint8_t total = 1 + (ENABLE_IG ? 1 : 0) + customMsgCount;
  slideIndex = (slideIndex + 1) % total;
  slideAnimDone = false;

  if (slideIndex == 0) {
    // YouTube
    flap.setNumericMode(true);
    drawLogoArea(true);
    flap.setTarget(fmtCountCompact(ytSubs).c_str());
    return;
  }

  if (ENABLE_IG && slideIndex == 1) {
    // Instagram
    flap.setNumericMode(true);
    drawLogoArea(false);
    flap.setTarget(fmtCountCompact(igFollowers).c_str());
    return;
  }

  // Custom messages — clear icon area
  uint8_t msgIdx = slideIndex - 1 - (ENABLE_IG ? 1 : 0);
  if (msgIdx < customMsgCount) {
    flap.setNumericMode(false);
    // Clear icon area to background color
    CRGB bg = CRGB((colorBg >> 16) & 0xFF, (colorBg >> 8) & 0xFF, colorBg & 0xFF);
    for (int y = 0; y < MATRIX_H; y++)
      for (int x = 0; x < FLAP_START_X; x++)
        setPixel(x, y, bg);
    flap.setTarget(customMsgs[msgIdx].c_str());
  }
}

uint32_t currentSlideHoldSec() {
  uint8_t customStart = 1 + (ENABLE_IG ? 1 : 0);
  if (slideIndex < customStart) return displayTimeSec;
  uint8_t msgIdx = slideIndex - customStart;
  if (msgIdx < customMsgCount) return customMsgHoldSec[msgIdx];
  return displayTimeSec;
}

// =====================================================================
//  Loop
// =====================================================================
void loop() {
  server.handleClient();
  pollButtons();

  // Periodic fetch
  if (lastFetch == 0 || millis() - lastFetch >= (uint32_t)fetchPeriodSec * 1000UL) {
    lastFetch = millis();
    checkAutoRefreshToken();
    ytSubs      = fetchYouTubeSubs();
    igFollowers = ENABLE_IG ? fetchInstagramFollowers() : -1;
  }

  // Splitflap animation
  static uint32_t lastFrame = 0;
  if (millis() - lastFrame >= flipSpeedMs) {
    lastFrame = millis();
    bool wasBusy = flap.isBusy();
    flap.update();
    flap.draw();
    FastLED.show();

    if (flap.didFlap()) flapClick();

    bool nowBusy = flap.isBusy();
    if (wasBusy && !nowBusy && !slideAnimDone) {
      slideAnimDone = true;
      slideHoldStart = millis();
    }
  }

  // Slide rotation
  if (triggerSwitch) {
    triggerSwitch = false;
    advanceSlide();
  } else if (slideAnimDone) {
    uint32_t holdMs = currentSlideHoldSec() * 1000UL;
    if (millis() - slideHoldStart >= holdMs) {
      advanceSlide();
    }
  }
}
