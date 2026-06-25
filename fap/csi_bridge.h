// csi_bridge.h - Flipper Zero FAP: transparent bridge between the ESP32-S2's
// UART (CSI frames) and the phone over the Flipper's own BLE serial.
//
//   ESP32-S2 ──UART──► Flipper ──BLE serial──► phone app
//   phone app ──BLE──► Flipper ──UART──► ESP32-S2   (control bytes)
//
// The bridge is byte-transparent: it does not parse CSI frames. The phone
// reassembles frames from the byte stream using the 0xC5 magic + length.
#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <gui/gui.h>
#include <input/input.h>
#include <bt/bt_service/bt.h>

#define TAG "CsiBridge"

// UART that the ESP32-S2 is wired to on the Flipper header:
//   FuriHalSerialIdUsart  = pins 13(TX)/14(RX)  (default Marauder dev board)
//   FuriHalSerialIdLpuart = pins 15(TX)/16(RX)
// Set this to whichever pins your multiboard's ESP32 uses (the set that
// answered when you bridged it).
#define CSI_UART_ID    FuriHalSerialIdLpuart
#define CSI_UART_BAUD  115200u

#define UART_RX_STREAM_SIZE 2048
#define BT_SERIAL_RX_BUFFER 256
#define BT_TX_CHUNK         200  // max bytes per BLE serial tx call

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* input_queue;
    Gui* gui;
    ViewPort* view_port;
    Bt* bt;

    FuriHalSerialHandle* serial;
    FuriStreamBuffer* uart_rx_stream;  // ESP32 -> (worker) -> BLE
    FuriThread* worker;

    volatile bool running;
    volatile bool bt_connected;
    uint32_t bytes_to_phone;
    uint32_t bytes_to_esp;
} CsiBridge;
