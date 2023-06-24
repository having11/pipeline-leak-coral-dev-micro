#include <cstdio>
#include <string>

#include "libs/base/led.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"
#include "libs/base/ipc_m4.h"

#include "IPC_message.h"

using namespace coralmicro;

static IpcM4* ipc;

void handleM7Message(const uint8_t data[kIpcMessageBufferDataSize]) {
    const auto* msg = reinterpret_cast<const msg::Message*>(data);

    IpcMessage ackMsg{};
    ackMsg.type = IpcMessageType::kApp;
    auto* appMsg = reinterpret_cast<msg::Message*>(&ackMsg.message.data);
    appMsg->type = msg::MessageType::kAck;
    ipc->SendMessage(ackMsg);
}

// Grab audio sample from mic

// Send audio to yamnet model and check if "water" sounding

// Notify M7 that target sound was detected

extern "C" [[noreturn]] void app_main(void *param) {
    (void)param;
    printf("[M4] Started\r\n");

    ipc = IpcM4::GetSingleton();
    ipc->RegisterAppMessageHandler(handleM7Message);

    

    vTaskSuspend(nullptr);
}
