// csi_bridge.c - see csi_bridge.h
//
// Built against the Momentum/OFW API (~v86). BLE-serial usage is modeled on
// maybe-hello-world/fbs; UART usage on the furi_hal_serial async API used by the
// WiFi Marauder companion app. Compile with ufbt; a couple of furi_hal symbols
// are firmware-version sensitive (noted inline) — fix names if the SDK differs.
#include "csi_bridge.h"

// ----------------------------------------------------------------- GUI
static void draw_callback(Canvas* canvas, void* ctx) {
    CsiBridge* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "CSI BLE Bridge");
    canvas_set_font(canvas, FontSecondary);

    char line[48];
    snprintf(line, sizeof(line), "BLE: %s", app->bt_connected ? "phone connected" : "advertising...");
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, line);
    snprintf(line, sizeof(line), "ESP->phone: %lu B", (unsigned long)app->bytes_to_phone);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, line);
    snprintf(line, sizeof(line), "phone->ESP: %lu B", (unsigned long)app->bytes_to_esp);
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, line);

    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* input, void* ctx) {
    CsiBridge* app = ctx;
    furi_message_queue_put(app->input_queue, input, FuriWaitForever);
}

// ---------------------------------------------------- BT connection status
static void bt_status_callback(BtStatus status, void* ctx) {
    CsiBridge* app = ctx;
    app->bt_connected = (status == BtStatusConnected);
}

// ---------------------------------------------------- BLE serial RX (phone -> ESP)
// Phone wrote bytes to the Flipper's serial RX characteristic; forward to the ESP32.
static uint16_t bt_serial_event_callback(SerialServiceEvent event, void* context) {
    CsiBridge* app = context;
    if(event.event == SerialServiceEventTypeDataReceived) {
        furi_hal_serial_tx(app->serial, event.data.buffer, event.data.size);
        app->bytes_to_esp += event.data.size;
    }
    return 0;
}

// ---------------------------------------------------- UART RX (ESP -> stream)
static void uart_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent ev, void* context) {
    CsiBridge* app = context;
    if(ev == FuriHalSerialRxEventData) {
        uint8_t b = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(app->uart_rx_stream, &b, 1, 0);
    }
}

// ---------------------------------------------------- worker: stream -> BLE
static int32_t bridge_worker(void* context) {
    CsiBridge* app = context;
    uint8_t buf[BT_TX_CHUNK];
    while(app->running) {
        size_t n = furi_stream_buffer_receive(app->uart_rx_stream, buf, sizeof(buf), 50);
        if(n == 0) continue;
        if(!app->bt_connected) continue;  // drop while no phone is connected
        // furi_hal_bt_serial_tx returns false if the BLE tx buffer is full; retry briefly.
        size_t off = 0;
        while(off < n && app->running) {
            uint16_t chunk = (uint16_t)(n - off);
            if(ble_profile_serial_tx(app->ble_profile, buf + off, chunk)) {
                off += chunk;
                app->bytes_to_phone += chunk;
            } else {
                furi_delay_ms(2);
            }
        }
    }
    return 0;
}

// ----------------------------------------------------------------- lifecycle
static CsiBridge* csi_bridge_alloc(void) {
    CsiBridge* app = malloc(sizeof(CsiBridge));
    memset(app, 0, sizeof(CsiBridge));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->uart_rx_stream = furi_stream_buffer_alloc(UART_RX_STREAM_SIZE, 1);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->bt = furi_record_open(RECORD_BT);
    return app;
}

static void csi_bridge_free(CsiBridge* app) {
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_BT);
    furi_stream_buffer_free(app->uart_rx_stream);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t csi_bridge_app(void* p) {
    UNUSED(p);
    CsiBridge* app = csi_bridge_alloc();

    // --- UART to the ESP32-S2 ---
    app->serial = furi_hal_serial_control_acquire(CSI_UART_ID);
    if(app->serial) {
        furi_hal_serial_init(app->serial, CSI_UART_BAUD);
        furi_hal_serial_async_rx_start(app->serial, uart_rx_callback, app, false);
    } else {
        FURI_LOG_E(TAG, "Could not acquire UART %d", CSI_UART_ID);
    }

    // --- worker thread (UART stream -> BLE) ---
    app->running = true;
    app->worker = furi_thread_alloc_ex("CsiBridgeWorker", 2048, bridge_worker, app);
    furi_thread_start(app->worker);

    // --- BLE: start our serial profile + advertise as "Flipper <name>" ---
    bt_set_status_changed_callback(app->bt, bt_status_callback, app);
    BleProfileSerialParams params = {.device_name_prefix = NULL, .mac_xor = 0};
    app->ble_profile = bt_profile_start(app->bt, ble_profile_serial, &params);
    furi_hal_bt_start_advertising();
    if(app->ble_profile) {
        ble_profile_serial_set_event_callback(
            app->ble_profile, BT_SERIAL_RX_BUFFER, bt_serial_event_callback, app);
    } else {
        FURI_LOG_E(TAG, "Failed to start BLE serial profile (is Bluetooth enabled?)");
    }

    // --- input loop ---
    InputEvent event;
    for(bool processing = true; processing;) {
        if(furi_message_queue_get(app->input_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypePress && event.key == InputKeyBack) processing = false;
        }
        view_port_update(app->view_port);
    }

    // --- teardown ---
    app->running = false;
    furi_thread_join(app->worker);
    furi_thread_free(app->worker);

    bt_set_status_changed_callback(app->bt, NULL, NULL);
    bt_profile_restore_default(app->bt);

    if(app->serial) {
        furi_hal_serial_async_rx_stop(app->serial);
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
    }

    csi_bridge_free(app);
    return 0;
}
