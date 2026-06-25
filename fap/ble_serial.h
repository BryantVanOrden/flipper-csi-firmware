/*
 * Vendored BLE serial profile (based on the helper used by flipper-pc-monitor /
 * ami_tool). Brings its own serial profile so RX callbacks reach the app reliably
 * on current firmware (Momentum / OFW dev).
 */

#pragma once

#include <furi_ble/profile_interface.h>
#include <services/serial_service.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* device_name_prefix;
    uint16_t mac_xor;
} BleProfileSerialParams;

#define BLE_PROFILE_SERIAL_PACKET_SIZE_MAX BLE_SVC_SERIAL_DATA_LEN_MAX

typedef SerialServiceEventCallback FuriHalBtSerialCallback;

extern const FuriHalBleProfileTemplate* const ble_profile_serial;

bool ble_profile_serial_tx(FuriHalBleProfileBase* profile, uint8_t* data, uint16_t size);

void ble_profile_serial_notify_buffer_is_empty(FuriHalBleProfileBase* profile);

void ble_profile_serial_set_event_callback(
    FuriHalBleProfileBase* profile,
    uint16_t buff_size,
    FuriHalBtSerialCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
