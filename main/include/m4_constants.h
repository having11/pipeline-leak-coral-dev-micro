#ifndef M4_CONSTANTS_H
#define M4_CONSTANTS_H

#include "libs/tensorflow/audio_models.h"

namespace M4Constant {
    using namespace coralmicro;

    constexpr int kTensorArenaSize = 1 * 1024 * 1024;
    constexpr int kNumDmaBuffers = 2;
    constexpr int kDmaBufferSizeMs = 50;
    constexpr int kDmaBufferSize =
        kNumDmaBuffers * tensorflow::kYamnetSampleRateMs * kDmaBufferSizeMs;
    constexpr int kAudioServicePriority = 4;
    constexpr int kDropFirstSamplesMs = 150;

    constexpr int kAudioBufferSizeMs = tensorflow::kYamnetDurationMs;
    constexpr int kAudioBufferSize =
        kAudioBufferSizeMs * tensorflow::kYamnetSampleRateMs;

    constexpr float kThreshold = 0.3;
    constexpr int kTopK = 5;

    constexpr char kModelName[] = "/models/yamnet_spectra_in.tflite";
    constexpr bool kUseTpu = false;
}

#endif