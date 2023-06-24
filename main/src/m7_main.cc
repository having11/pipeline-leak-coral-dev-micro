#include <cstdio>

#include "libs/base/led.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"
#include "libs/base/ipc_m7.h"
#include "libs/base/check.h"

#include "ipc_message.h"

using namespace coralmicro;

static IpcM7 *ipc;

void handleM4Message(const uint8_t data[kIpcMessageBufferDataSize]) {
    const auto* msg = reinterpret_cast<const msg::Message*>(data);

    printf("[M7] Received message type=%d\r\n", (uint8_t)msg->type);

    if (msg->type == msg::MessageType::kKeywordSpotted) {
        printf("[M7] KWS value=%d\r\n", msg->data.audioFound.found);

        // Check if sound has been detected by M4

        // Start inferencing on images
    }
}

// Grab image

// Pass image to object detection model

// Send image via HTTP

// Set up stream server
void init() {
    // Turn on Status LED to show the board is on.
    LedSet(Led::kStatus, true);

    // Init M4 core
    ipc = IpcM7::GetSingleton();
    ipc->RegisterAppMessageHandler(handleM4Message);
    ipc->StartM4();
    CHECK(ipc->M4IsAlive(500u));
}

[[noreturn]] void main() {
    init();

    while (true) {

    }
}

extern "C" [[noreturn]] void app_main(void *param) {
    (void)param;
    main();
}