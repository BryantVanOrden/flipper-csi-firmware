// csi_sense.cpp - see csi_sense.h
#include "csi_sense.h"

#include <WiFi.h>
#include <math.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"

// ---- raw CSI handed off from the WiFi task to loop() ----------------------
typedef struct {
  int8_t   buf[MAX_SUBCARRIERS * 2];  // interleaved [imag, real] int8 pairs
  uint16_t len;
  int8_t   rssi;
} csi_raw_t;

static QueueHandle_t s_queue = nullptr;

// ---- detector state --------------------------------------------------------
static float   s_baseline[MAX_SUBCARRIERS];  // EWMA empty-room amplitude
static bool    s_baselineInit = false;
static float   s_ampScale     = 1.0f;         // adaptive normalization scale
static float   s_motionEma    = 0.0f;
static float   s_presenceEma  = 0.0f;
static uint8_t s_motionThresh = DEFAULT_MOTION_THRESHOLD;
static uint16_t s_calCount    = 0;
static uint8_t s_seq          = 0;

// ---- breathing (experimental) ---------------------------------------------
static float    s_breathBuf[BREATH_WINDOW];
static uint16_t s_breathHead = 0;
static uint16_t s_breathFill = 0;
static uint8_t  s_breathBpm  = 0;
static uint16_t s_sinceBreath = 0;

// ---- mode / rate -----------------------------------------------------------
static uint8_t  s_mode    = DEFAULT_MODE;
static uint8_t  s_channel = DEFAULT_CHANNEL;
static int8_t   s_rssi    = 0;
static uint8_t  s_nsub    = 0;
static float    s_rate    = 0.0f;
static uint32_t s_rateWindowStart = 0;
static uint32_t s_rateCount = 0;

static esp_ping_handle_t s_ping = nullptr;

// ---------------------------------------------------------------------------
// CSI receive callback. Runs in the WiFi task: do the minimum (copy + enqueue).
// ---------------------------------------------------------------------------
static void IRAM_ATTR csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
  if (!info || !info->buf || !s_queue) return;
  csi_raw_t raw;
  uint16_t len = info->len;
  if (len > sizeof(raw.buf)) len = sizeof(raw.buf);
  memcpy(raw.buf, info->buf, len);
  raw.len  = len;
  raw.rssi = info->rx_ctrl.rssi;
  xQueueSend(s_queue, &raw, 0);  // non-blocking; drop if loop() is behind
}

static void enableCsi() {
  wifi_csi_config_t csi_config = {};
  csi_config.lltf_en           = true;
  csi_config.htltf_en          = true;
  csi_config.stbc_htltf2_en    = true;
  csi_config.ltf_merge_en      = true;
  csi_config.channel_filter_en = true;
  csi_config.manu_scale        = false;
  csi_config.shift             = 0;
  esp_wifi_set_csi_config(&csi_config);
  esp_wifi_set_csi_rx_cb(&csi_rx_cb, nullptr);
  esp_wifi_set_csi(true);
}

// ---------------------------------------------------------------------------
// Packet sources
// ---------------------------------------------------------------------------
static void startPing() {
  if (s_ping) { esp_ping_stop(s_ping); esp_ping_delete_session(s_ping); s_ping = nullptr; }
  ip_addr_t target;
  IPAddress gw = WiFi.gatewayIP();
  ipaddr_aton(gw.toString().c_str(), &target);

  esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
  cfg.target_addr = target;
  cfg.interval_ms = PING_INTERVAL_MS;
  cfg.count       = ESP_PING_COUNT_INFINITE;
  cfg.timeout_ms  = 1000;
  esp_ping_callbacks_t cbs = {};  // we don't care about replies, only the RX traffic
  if (esp_ping_new_session(&cfg, &cbs, &s_ping) == ESP_OK) {
    esp_ping_start(s_ping);
  }
}

static char s_ssid[33] = WIFI_SSID;
static char s_pass[64] = WIFI_PASSWORD;

static void startActive() {
  if (s_ping) { esp_ping_stop(s_ping); esp_ping_delete_session(s_ping); s_ping = nullptr; }
  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(s_ssid, s_pass);
  Serial.print("[csi] joining WiFi");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(250); Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    s_channel = WiFi.channel();
    Serial.printf("[csi] connected, ch=%u gw=%s\n", s_channel, WiFi.gatewayIP().toString().c_str());
    enableCsi();
    startPing();
  } else {
    Serial.println("[csi] WiFi connect failed - falling back to passive");
    s_mode = 0;
  }
}

static void startPassive() {
  if (s_ping) { esp_ping_stop(s_ping); esp_ping_delete_session(s_ping); s_ping = nullptr; }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);            // station mode, but we won't associate
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
  enableCsi();
  Serial.printf("[csi] passive sniff on ch=%u\n", s_channel);
}

static void applyMode() {
  s_calCount = 0;            // recalibrate whenever the source changes
  s_baselineInit = false;
  if (s_mode == 1) startActive();
  else             startPassive();
}

// ---------------------------------------------------------------------------
// Breathing estimate: crude autocorrelation of mean-amplitude over the window,
// searched only over lags corresponding to BREATH_MIN_BPM..BREATH_MAX_BPM.
// Returns 0 unless a confident periodic peak is found. EXPERIMENTAL.
// ---------------------------------------------------------------------------
static uint8_t estimateBreathing() {
  if (s_breathFill < BREATH_WINDOW || s_rate < 4.0f) return 0;

  // Pull the window into time order and remove the mean (detrend).
  static float w[BREATH_WINDOW];
  float mean = 0;
  for (uint16_t i = 0; i < BREATH_WINDOW; i++) {
    w[i] = s_breathBuf[(s_breathHead + i) % BREATH_WINDOW];
    mean += w[i];
  }
  mean /= BREATH_WINDOW;
  float energy = 0;
  for (uint16_t i = 0; i < BREATH_WINDOW; i++) { w[i] -= mean; energy += w[i] * w[i]; }
  if (energy < 1e-3f) return 0;

  // Convert the bpm band into a lag band (in samples).
  int lagMin = (int)(s_rate * 60.0f / BREATH_MAX_BPM);
  int lagMax = (int)(s_rate * 60.0f / BREATH_MIN_BPM);
  if (lagMax >= BREATH_WINDOW) lagMax = BREATH_WINDOW - 1;
  if (lagMin < 2) lagMin = 2;

  float bestCorr = 0; int bestLag = 0;
  for (int lag = lagMin; lag <= lagMax; lag++) {
    float c = 0;
    for (uint16_t i = 0; i + lag < BREATH_WINDOW; i++) c += w[i] * w[i + lag];
    c /= energy;
    if (c > bestCorr) { bestCorr = c; bestLag = lag; }
  }
  if (bestCorr < 0.30f || bestLag == 0) return 0;   // not confidently periodic
  int bpm = (int)roundf(s_rate * 60.0f / bestLag);
  if (bpm < BREATH_MIN_BPM || bpm > BREATH_MAX_BPM) return 0;
  return (uint8_t)bpm;
}

// ---------------------------------------------------------------------------
// Process one raw CSI record into detector state.
// ---------------------------------------------------------------------------
static void processRaw(const csi_raw_t &raw, CsiResult &out) {
  int n = raw.len / 2;
  if (n > MAX_SUBCARRIERS) n = MAX_SUBCARRIERS;
  if (n <= 0) return;
  s_nsub = n;
  s_rssi = raw.rssi;

  float amp[MAX_SUBCARRIERS];
  float frameMax = 1.0f, meanAmp = 0.0f;
  for (int i = 0; i < n; i++) {
    int8_t imag = raw.buf[2 * i];       // ESP32 CSI: [imag, real] per subcarrier
    int8_t real = raw.buf[2 * i + 1];
    float a = sqrtf((float)imag * imag + (float)real * real);
    amp[i] = a;
    meanAmp += a;
    if (a > frameMax) frameMax = a;
  }
  meanAmp /= n;

  // Adaptive normalization scale (slow decay toward the running peak).
  s_ampScale *= 0.999f;
  if (frameMax > s_ampScale) s_ampScale = frameMax;
  if (s_ampScale < 1.0f) s_ampScale = 1.0f;

  // Calibration: build the baseline quickly before we trust any detection.
  if (!s_baselineInit) {
    for (int i = 0; i < n; i++) s_baseline[i] = amp[i];
    s_baselineInit = true;
  }
  bool calibrating = s_calCount < CALIBRATION_SAMPLES;
  float alpha = calibrating ? 0.2f : BASELINE_ALPHA;

  float devSum = 0;
  for (int i = 0; i < n; i++) {
    devSum += fabsf(amp[i] - s_baseline[i]);
    s_baseline[i] += alpha * (amp[i] - s_baseline[i]);
  }
  float devMean = devSum / n;

  // Normalize deviation to 0..255 against the current amplitude scale.
  float motion = (devMean / s_ampScale) * 255.0f * MOTION_GAIN;
  if (motion > 255.0f) motion = 255.0f;
  if (motion < 0.0f)   motion = 0.0f;

  s_motionEma   += MOTION_EMA_ALPHA   * (motion - s_motionEma);
  s_presenceEma += PRESENCE_EMA_ALPHA * (motion - s_presenceEma);

  if (calibrating) { s_calCount++; }

  // Breathing buffer + occasional estimate (only when fairly still).
  s_breathBuf[s_breathHead] = meanAmp;
  s_breathHead = (s_breathHead + 1) % BREATH_WINDOW;
  if (s_breathFill < BREATH_WINDOW) s_breathFill++;
  if (++s_sinceBreath >= 32) {
    s_sinceBreath = 0;
    s_breathBpm = (s_motionEma < s_motionThresh) ? estimateBreathing() : 0;
  }

  // Sample-rate measurement (1 s window).
  s_rateCount++;
  uint32_t now = millis();
  if (s_rateWindowStart == 0) s_rateWindowStart = now;
  if (now - s_rateWindowStart >= 1000) {
    s_rate = s_rateCount * 1000.0f / (now - s_rateWindowStart);
    s_rateCount = 0;
    s_rateWindowStart = now;
  }

  // Fill the result.
  out.n_sub = n;
  for (int i = 0; i < n; i++) {
    int v = (int)(amp[i] / s_ampScale * 255.0f);
    out.amp[i] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
  }
  out.motion        = (uint8_t)s_motionEma;
  out.motion_flag   = s_motionEma > s_motionThresh;
  out.presence      = !calibrating && s_presenceEma > PRESENCE_THRESHOLD;
  out.breathing_bpm = s_breathBpm;
  out.rssi          = s_rssi;
  out.calibrated    = !calibrating;
  out.seq           = s_seq++;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void CsiSense::begin(uint8_t mode, uint8_t channel) {
  s_mode = mode; s_channel = channel;
  if (!s_queue) s_queue = xQueueCreate(32, sizeof(csi_raw_t));
  applyMode();
}

bool CsiSense::poll(CsiResult &out) {
  csi_raw_t raw;
  bool got = false;
  // Drain everything queued; the caller gets the most recent processed result.
  while (xQueueReceive(s_queue, &raw, 0) == pdTRUE) {
    processRaw(raw, out);
    got = true;
  }
  return got;
}

void CsiSense::setMode(uint8_t mode)  { if (mode <= 1 && mode != s_mode) { s_mode = mode; applyMode(); } }
void CsiSense::setChannel(uint8_t ch) { if (ch >= 1 && ch <= 13) { s_channel = ch; if (s_mode == 0) startPassive(); } }
void CsiSense::setMotionThreshold(uint8_t t) { s_motionThresh = t; }
void CsiSense::resetBaseline()        { s_calCount = 0; s_baselineInit = false; s_breathFill = 0; s_breathHead = 0; }

void CsiSense::setCredentials(const char* ssid, const char* pass) {
  if (ssid) { strncpy(s_ssid, ssid, sizeof(s_ssid) - 1); s_ssid[sizeof(s_ssid) - 1] = 0; }
  if (pass) { strncpy(s_pass, pass, sizeof(s_pass) - 1); s_pass[sizeof(s_pass) - 1] = 0; }
}
void CsiSense::applyCredentials()     { s_mode = 1; applyMode(); }

uint8_t CsiSense::mode()        { return s_mode; }
uint8_t CsiSense::channel()     { return s_channel; }
float   CsiSense::sampleRate()  { return s_rate; }
int8_t  CsiSense::lastRssi()    { return s_rssi; }
uint8_t CsiSense::subcarriers() { return s_nsub; }
bool    CsiSense::calibrated()  { return s_calCount >= CALIBRATION_SAMPLES; }
