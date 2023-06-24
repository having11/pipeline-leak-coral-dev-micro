#ifndef IPC_MESSAGE_H
#define IPC_MESSAGE_H

namespace msg {

#include "libs/base/ipc_message_buffer.h"

enum class MessageType : uint8_t {
    kKeywordSpotted,
    kAck
};

struct MessageAudioFound {
    bool found;
};

union MessageData {
    MessageAudioFound audioFound;
};

struct Message {
    MessageType type;
    MessageData data;
} __attribute__((packed));

static_assert(sizeof(Message) <=
              coralmicro::kIpcMessageBufferDataSize);

}

#endif