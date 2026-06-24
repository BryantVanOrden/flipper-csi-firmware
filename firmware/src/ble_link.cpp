// ble_link.cpp - see ble_link.h
#include "ble_link.h"
#include "config.h"
#include <NimBLEDevice.h>

static NimBLEServer*         s_server     = nullptr;
static NimBLECharacteristic* s_csiChar    = nullptr;
static NimBLECharacteristic* s_ctrlChar   = nullptr;
static NimBLECharacteristic* s_statusChar = nullptr;
static volatile bool         s_connected  = false;
static BleLink::CtrlCb       s_ctrlCb     = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server) override {
    s_connected = true;
    Serial.println("[ble] central connected");
  }
  void onDisconnect(NimBLEServer* server) override {
    s_connected = false;
    Serial.println("[ble] central disconnected, advertising again");
    NimBLEDevice::startAdvertising();
  }
};

class CtrlCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    std::string v = c->getValue();
    if (v.empty() || !s_ctrlCb) return;
    uint8_t cmd = (uint8_t)v[0];
    uint8_t arg = (v.size() > 1) ? (uint8_t)v[1] : 0;
    s_ctrlCb(cmd, arg, v.size() > 1);
  }
};

void BleLink::begin(CtrlCb cb) {
  s_ctrlCb = cb;

  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEDevice::setMTU(247);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  s_server = NimBLEDevice::createServer();
  s_server->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = s_server->createService(SVC_UUID);

  s_csiChar = svc->createCharacteristic(CHR_CSI_UUID, NIMBLE_PROPERTY::NOTIFY);

  s_ctrlChar = svc->createCharacteristic(CHR_CTRL_UUID,
                 NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  s_ctrlChar->setCallbacks(new CtrlCallbacks());

  s_statusChar = svc->createCharacteristic(CHR_STATUS_UUID,
                 NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  s_statusChar->setValue("{}");

  svc->start();

  // The 128-bit service UUID (18 bytes) + the name won't both fit in the 31-byte
  // advertising packet, so put the UUID in the adv data and the name in the
  // scan-response packet. Android merges both on an active scan.
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();

  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);                       // LE General Discoverable, BR/EDR not supported
  advData.setCompleteServices(NimBLEUUID(SVC_UUID));
  adv->setAdvertisementData(advData);

  NimBLEAdvertisementData scanData;
  scanData.setName(BLE_DEVICE_NAME);
  adv->setScanResponseData(scanData);
  adv->setScanResponse(true);

  NimBLEDevice::startAdvertising();
  Serial.println("[ble] advertising as " BLE_DEVICE_NAME);
}

bool BleLink::isConnected() { return s_connected; }

void BleLink::notifyCsi(const CsiResult &r) {
  if (!s_connected || !s_csiChar) return;
  uint8_t n = r.n_sub;
  if (n > MAX_SUBCARRIERS) n = MAX_SUBCARRIERS;

  uint8_t buf[8 + MAX_SUBCARRIERS];
  buf[0] = PROTO_MAGIC;
  buf[1] = PROTO_VERSION;
  buf[2] = n;
  uint8_t flags = 0;
  if (r.presence)        flags |= 0x01;
  if (r.motion_flag)     flags |= 0x02;
  if (CsiSense::mode()==1) flags |= 0x04;
  buf[3] = flags;
  buf[4] = (uint8_t)r.rssi;        // int8 carried in a byte; app reinterprets
  buf[5] = r.motion;
  buf[6] = r.breathing_bpm;
  buf[7] = r.seq;
  memcpy(buf + 8, r.amp, n);

  s_csiChar->setValue(buf, 8 + n);
  s_csiChar->notify();
}

void BleLink::updateStatus(const char *json) {
  if (!s_statusChar) return;
  s_statusChar->setValue((uint8_t*)json, strlen(json));
  if (s_connected) s_statusChar->notify();
}
