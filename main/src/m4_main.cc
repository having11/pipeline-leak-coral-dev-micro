#include <cstdio>
#include <string>

#include "libs/audio/audio_service.h"
#include "libs/base/filesystem.h"
#include "libs/base/ipc_m4.h"
#include "libs/base/led.h"
#include "libs/base/timer.h"
#include "libs/tensorflow/audio_models.h"
#include "libs/tensorflow/utils.h"
#include "libs/tpu/edgetpu_manager.h"
#include "libs/tpu/edgetpu_op.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"
#include "third_party/tflite-micro/tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_interpreter.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_mutable_op_resolver.h"

#include "ipc_message.h"
#include "m4_constants.h"

using namespace coralmicro;

STATIC_TENSOR_ARENA_IN_SDRAM(tensor_arena, M4Constant::kTensorArenaSize);
static AudioDriverBuffers<M4Constant::kNumDmaBuffers, M4Constant::kDmaBufferSize> audio_buffers;
static AudioDriver audio_driver(audio_buffers);
static std::array<int16_t, tensorflow::kYamnetAudioSize> audio_input;

static IpcM4* ipc;

void handleM7Message(const uint8_t data[kIpcMessageBufferDataSize]) {
    const auto* msg = reinterpret_cast<const msg::Message*>(data);

    IpcMessage ackMsg{};
    ackMsg.type = IpcMessageType::kApp;
    auto* appMsg = reinterpret_cast<msg::Message*>(&ackMsg.message.data);
    appMsg->type = msg::MessageType::kAck;
    ipc->SendMessage(ackMsg);
}

// Run invoke and get the results after the interpreter have already been
// populated with raw audio input.
void run(tflite::MicroInterpreter* interpreter, FrontendState* frontend_state) {
    auto input_tensor = interpreter->input_tensor(0);
    auto preprocess_start = TimerMillis();
    tensorflow::YamNetPreprocessInput(audio_input.data(), input_tensor,
                                        frontend_state);
    // Reset frontend state.
    FrontendReset(frontend_state);
    auto preprocess_end = TimerMillis();
    if (interpreter->Invoke() != kTfLiteOk) {
        printf("Failed to invoke on test input\r\n");
        vTaskSuspend(nullptr);
    }
    auto current_time = TimerMillis();
    printf(
        "Yamnet preprocess time: %lums, invoke time: %lums, total: "
        "%lums\r\n",
        static_cast<uint32_t>(preprocess_end - preprocess_start),
        static_cast<uint32_t>(current_time - preprocess_end),
        static_cast<uint32_t>(current_time - preprocess_start));
    auto results =
        tensorflow::GetClassificationResults(interpreter, M4Constant::kThreshold, M4Constant::kTopK);
    printf("%s\r\n", tensorflow::FormatClassificationOutput(results).c_str());
}

[[noreturn]] void Main() {
    LedSet(Led::kStatus, true);

    std::vector<uint8_t> yamnet_tflite;
    if (!LfsReadFile(M4Constant::kModelName, &yamnet_tflite)) {
        printf("Failed to load model\r\n");
        vTaskSuspend(nullptr);
    }

    const auto* model = tflite::GetModel(yamnet_tflite.data());
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("Model schema version is %lu, supported is %d", model->version(),
            TFLITE_SCHEMA_VERSION);
        vTaskSuspend(nullptr);
    }

    tflite::MicroErrorReporter error_reporter;
    auto yamnet_resolver = tensorflow::SetupYamNetResolver<M4Constant::kUseTpu>();

    tflite::MicroInterpreter interpreter{model, yamnet_resolver, tensor_arena,
                                        M4Constant::kTensorArenaSize, &error_reporter};
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        printf("AllocateTensors failed.\r\n");
        vTaskSuspend(nullptr);
    }

    FrontendState frontend_state{};
    if (!coralmicro::tensorflow::PrepareAudioFrontEnd(
            &frontend_state, coralmicro::tensorflow::AudioModel::kYAMNet)) {
        printf("coralmicro::tensorflow::PrepareAudioFrontEnd() failed.\r\n");
        vTaskSuspend(nullptr);
    }

    // Run tensorflow on test input file.
    std::vector<uint8_t> yamnet_test_input_bin;
    if (!LfsReadFile("/models/yamnet_test_audio.bin", &yamnet_test_input_bin)) {
        printf("Failed to load test input!\r\n");
        vTaskSuspend(nullptr);
    }
    if (yamnet_test_input_bin.size() !=
        tensorflow::kYamnetAudioSize * sizeof(int16_t)) {
        printf("Input audio size doesn't match expected\r\n");
        vTaskSuspend(nullptr);
    }
    auto input_tensor = interpreter.input_tensor(0);
    std::memcpy(tflite::GetTensorData<uint8_t>(input_tensor),
                yamnet_test_input_bin.data(), yamnet_test_input_bin.size());
    run(&interpreter, &frontend_state);

    // Setup audio
    AudioDriverConfig audio_config{AudioSampleRate::k16000_Hz, M4Constant::kNumDmaBuffers,
                                    M4Constant::kDmaBufferSizeMs};
    AudioService audio_service(&audio_driver, audio_config, M4Constant::kAudioServicePriority,
                                M4Constant::kDropFirstSamplesMs);
    LatestSamples audio_latest(
        MsToSamples(AudioSampleRate::k16000_Hz, tensorflow::kYamnetDurationMs));
    audio_service.AddCallback(
        &audio_latest,
        +[](void* ctx, const int32_t* samples, size_t num_samples) {
            static_cast<LatestSamples*>(ctx)->Append(samples, num_samples);
            return true;
        });
    // Delay for the first buffers to fill.
    vTaskDelay(pdMS_TO_TICKS(tensorflow::kYamnetDurationMs));
    while (true) {
        audio_latest.AccessLatestSamples(
            [](const std::vector<int32_t>& samples, size_t start_index) {
            size_t i, j = 0;
            // Starting with start_index, grab until the end of the buffer.
            for (i = 0; i < samples.size() - start_index; ++i) {
                audio_input[i] = samples[i + start_index] >> 16;
            }
            // Now fill the rest of the data with the beginning of the
            // buffer.
            for (j = 0; j < samples.size() - i; ++j) {
                audio_input[i + j] = samples[j] >> 16;
            }
            });
        run(&interpreter, &frontend_state);
    }
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
