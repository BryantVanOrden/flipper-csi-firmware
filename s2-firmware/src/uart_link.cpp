// uart_link.cpp - see uart_link.h
#include "uart_link.h"
#include "config.h"

// We use UART0 (`Serial`), which on the dev board is wired to the Flipper header
// (build flag ARDUINO_USB_CDC_ON_BOOT=0 keeps Serial on the GPIO UART, not USB).

void UartLink::begin(uint32_t baud) {
  Serial.begin(baud);
}

void UartLink::sendCsi(const CsiResult &r) {
  uint8_t n = r.n_sub;
  if (n > MAX_SUBCARRIERS) n = MAX_SUBCARRIERS;

  uint8_t buf[8 + MAX_SUBCARRIERS];
  buf[0] = CSI_MAGIC;
  buf[1] = PROTO_VERSION;
  buf[2] = n;
  uint8_t flags = 0;
  if (r.presence)              flags |= 0x01;
  if (r.motion_flag)           flags |= 0x02;
  if (CsiSense::mode() == 1)   flags |= 0x04;
  buf[3] = flags;
  buf[4] = (uint8_t)r.rssi;    // int8 carried in a byte
  buf[5] = r.motion;
  buf[6] = r.breathing_bpm;
  buf[7] = r.seq;
  memcpy(buf + 8, r.amp, n);

  Serial.write(buf, 8 + n);
}

void UartLink::sendStatus(const char* json) {
  size_t len = strlen(json);
  if (len > 255) len = 255;
  uint8_t hdr[2] = { STATUS_MAGIC, (uint8_t)len };
  Serial.write(hdr, 2);
  Serial.write((const uint8_t*)json, len);
}

// State machine for [0xC7, cmd, len, payload[len]].
void UartLink::poll(CtrlCb cb) {
  static uint8_t state = 0;   // 0 idle, 1 got magic, 2 got cmd, 3 reading payload
  static uint8_t cmd = 0, len = 0, idx = 0;
  static uint8_t payload[64];
  while (Serial.available() > 0) {
    uint8_t b = (uint8_t)Serial.read();
    switch (state) {
      case 0: if (b == CTRL_MAGIC) state = 1; break;
      case 1: cmd = b; state = 2; break;
      case 2:
        len = b; idx = 0;
        if (len == 0) { if (cb) cb(cmd, payload, 0); state = 0; }
        else state = 3;
        break;
      case 3:
        if (idx < sizeof(payload)) payload[idx] = b;
        idx++;
        if (idx >= len) {
          uint8_t n = (len < sizeof(payload)) ? len : (uint8_t)sizeof(payload);
          if (cb) cb(cmd, payload, n);
          state = 0;
        }
        break;
    }
  }
}
