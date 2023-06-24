#include <cstdio>
#include <vector>
#include <memory>

#include "libs/audio/audio_service.h"
#include "libs/base/check.h"
#include "libs/base/filesystem.h"
#include "libs/base/http_server.h"
#include "libs/base/ipc_m7.h"
#include "libs/base/led.h"
#include "libs/base/strings.h"
#include "libs/base/timer.h"
#include "libs/base/utils.h"
#include "libs/base/wifi.h"
#include "libs/camera/camera.h"
#include "libs/libjpeg/jpeg.h"
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
#include "m7_constants.h"

using namespace coralmicro;

static IpcM7 *ipc;
SemaphoreHandle_t xCameraSema;
StaticSemaphore_t xCameraSemaBuffer;

STATIC_TENSOR_ARENA_IN_SDRAM(tensor_arena, M7Constant::kTensorArenaSize);
static AudioDriverBuffers<M7Constant::kNumDmaBuffers, M7Constant::kDmaBufferSize> audio_buffers;
static AudioDriver audio_driver(audio_buffers);
static std::array<int16_t, tensorflow::kYamnetAudioSize> audio_input;
static std::unique_ptr<tflite::MicroInterpreter> interpreter;
static std::unique_ptr<FrontendState> frontend_state;
static std::unique_ptr<LatestSamples> audio_latest;

void handleM4Message(const uint8_t data[kIpcMessageBufferDataSize]) {
    const auto* msg = reinterpret_cast<const msg::Message*>(data);

    printf("[M7] Received message type=%d\r\n", (uint8_t)msg->type);

    if (msg->type == msg::MessageType::kKeywordSpotted) {
        printf("[M7] KWS value=%d\r\n", msg->data.audioFound.found);

        // Check if sound has been detected by M4

        // Start inferencing on images
    }

    else if (msg->type == msg::MessageType::kAck) {
        printf("[M7] M4 ACK message\r\n");
    }
}

HttpServer::Content uriHandler(const char* uri) {
    if (StrEndsWith(uri, M7Constant::kCameraStreamUrlPrefix)) {
        std::vector<uint8_t> buf(M7Constant::kWidth * M7Constant::kHeight *
                                CameraFormatBpp(M7Constant::kFormat));
        auto fmt = CameraFrameFormat{
            M7Constant::kFormat,       M7Constant::kFilter,
            M7Constant::kRotation,     M7Constant::kWidth,
            M7Constant::kHeight,
            /*preserve_ratio=*/false, buf.data(),
            /*while_balance=*/true};
        // Grab image
        if (xSemaphoreTake(xCameraSema, pdMS_TO_TICKS(M7Constant::kMaxCameraWaitMs)) == pdTRUE) {
            if (!CameraTask::GetSingleton()->GetFrame({fmt})) {
                printf("Unable to get frame from camera\r\n");
                return "Unable to get frame from camera";
            }

            xSemaphoreGive(xCameraSema);

            std::vector<uint8_t> jpeg;
            JpegCompressRgb(buf.data(), fmt.width, fmt.height, M7Constant::kJpegQuality, &jpeg);
            return jpeg;
        } else {
            printf("Timeout waiting for camera\r\n");
            return "Timeout waiting for camera";
        }
    }
    return {};
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
        tensorflow::GetClassificationResults(interpreter, M7Constant::kThreshold, M7Constant::kTopK);
    printf("%s\r\n", tensorflow::FormatClassificationOutput(results).c_str());
}

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

    std::vector<uint8_t> yamnet_tflite;
    if (!LfsReadFile(M7Constant::kModelName, &yamnet_tflite)) {
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
    auto yamnet_resolver = tensorflow::SetupYamNetResolver<M7Constant::kUseTpu>();

    interpreter = std::make_unique<tflite::MicroInterpreter>(model, yamnet_resolver, tensor_arena,
                                        M7Constant::kTensorArenaSize, &error_reporter);
    frontend_state = std::make_unique<FrontendState>();
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("AllocateTensors failed.\r\n");
        vTaskSuspend(nullptr);
    }

    if (!coralmicro::tensorflow::PrepareAudioFrontEnd(
            frontend_state.get(), coralmicro::tensorflow::AudioModel::kYAMNet)) {
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
    auto input_tensor = interpreter->input_tensor(0);
    std::memcpy(tflite::GetTensorData<uint8_t>(input_tensor),
                yamnet_test_input_bin.data(), yamnet_test_input_bin.size());
    run(interpreter.get(), frontend_state.get());

    // Setup audio
    AudioDriverConfig audio_config{AudioSampleRate::k16000_Hz, M7Constant::kNumDmaBuffers,
                                    M7Constant::kDmaBufferSizeMs};
    AudioService audio_service(&audio_driver, audio_config, M7Constant::kAudioServicePriority,
                                M7Constant::kDropFirstSamplesMs);
    audio_latest = std::make_unique<LatestSamples>(
        MsToSamples(AudioSampleRate::k16000_Hz, tensorflow::kYamnetDurationMs));
    audio_service.AddCallback(
        audio_latest.get(),
        +[](void* ctx, const int32_t* samples, size_t num_samples) {
            static_cast<LatestSamples*>(ctx)->Append(samples, num_samples);
            return true;
        });
    // Delay for the first buffers to fill.
    vTaskDelay(pdMS_TO_TICKS(tensorflow::kYamnetDurationMs));

    CameraTask::GetSingleton()->SetPower(true);
    CameraTask::GetSingleton()->Enable(M7Constant::kMode);

    if (!WiFiTurnOn(false)) {
        printf("Unable to bring up WiFi...\r\n");
        vTaskSuspend(nullptr);
    }
    if (!WiFiConnect(10)) {
        printf("Unable to connect to WiFi...\r\n");
        vTaskSuspend(nullptr);
    }
    if (auto wifi_ip = WiFiGetIp()) {
        printf("Serving on: %s\r\n", wifi_ip->c_str());
    } else {
        printf("Failed to get Wifi Ip\r\n");
        vTaskSuspend(nullptr);
    }

    HttpServer http_server;
    http_server.AddUriHandler(uriHandler);
    UseHttpServer(&http_server);
}

[[noreturn]] void main() {
    init();

    while (true) {
        audio_latest->AccessLatestSamples(
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
        run(interpreter.get(), frontend_state.get());
    }
}

extern "C" [[noreturn]] void app_main(void *param) {
    (void)param;

    xCameraSema = xSemaphoreCreateBinaryStatic(&xCameraSemaBuffer);

    main();
}