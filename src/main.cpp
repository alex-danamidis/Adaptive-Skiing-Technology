#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <vector>
#include <string>
#include <cstring>
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

using namespace std;

#pragma pack(push, 1)
struct WavHeader {
    char riff[4];
    uint32_t overall_size;
    char wave[4];
    char fmt_chunk[4];
    uint32_t length_fmt;
    uint16_t format_type;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_chunk[4];
    uint32_t data_size;
};
#pragma pack(pop)

struct AudioBufferSignal {
    const float* buffer;
    size_t total_samples;
};

AudioBufferSignal g_audio_signal = {nullptr, 0};

// Pre-allocated buffer in 8MB PSRAM to prevent RAM exhaustion
static float* psram_audio_buffer = nullptr;

int raw_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        if ((offset + i) < g_audio_signal.total_samples) {
            out_ptr[i] = g_audio_signal.buffer[offset + i];
        } else {
            out_ptr[i] = 0.0f;
        }
    }
    return EIDSP_OK;
}

bool read_wav_file(const char* path, size_t& out_num_samples) {
    File wav = LittleFS.open(path, "r");
    if (!wav) {
        Serial.printf("Error: Could not open file: %s\n", path);
        return false;
    }

    WavHeader header;
    if (wav.read(reinterpret_cast<uint8_t*>(&header), sizeof(WavHeader)) != sizeof(WavHeader)) {
        Serial.printf("Error: File too short to contain a valid WAV header: %s\n", path);
        wav.close();
        return false;
    }

    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        Serial.printf("Error: Not a valid RIFF/WAVE file: %s\n", path);
        wav.close();
        return false;
    }

    if (header.channels != 1 || header.bits_per_sample != 16) {
        Serial.printf("Error: Only 16-bit mono PCM WAV supported (found %d ch, %d bits)\n",
                      header.channels, header.bits_per_sample);
        wav.close();
        return false;
    }

    size_t num_samples = header.data_size / sizeof(int16_t);
    if (num_samples > EI_CLASSIFIER_RAW_SAMPLE_COUNT * 2) {
        num_samples = EI_CLASSIFIER_RAW_SAMPLE_COUNT * 2; // Cap to buffer limit
    }

    // Read int16 chunks and convert to float inside PSRAM
    int16_t temp_chunk[512];
    size_t samples_read = 0;

    while (samples_read < num_samples && wav.available()) {
        size_t to_read = min((size_t)512, num_samples - samples_read);
        size_t bytes_read = wav.read(reinterpret_cast<uint8_t*>(temp_chunk), to_read * sizeof(int16_t));
        size_t actual_samples = bytes_read / sizeof(int16_t);
        if (actual_samples == 0) break;

        for (size_t i = 0; i < actual_samples; i++) {
            psram_audio_buffer[samples_read + i] = static_cast<float>(temp_chunk[i]);
        }
        samples_read += actual_samples;
    }

    wav.close();
    out_num_samples = samples_read;
    return true;
}

void classify_file(const char* file_path) {
    size_t num_samples = 0;
    if (!read_wav_file(file_path, num_samples)) {
        return;
    }

    g_audio_signal.buffer = psram_audio_buffer;
    g_audio_signal.total_samples = num_samples;

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &raw_audio_signal_get_data;

    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        Serial.printf("Error: run_classifier failed on %s (code %d)\n", file_path, res);
        return;
    }

    const char* top_label = "";
    float top_score = 0.0f;

    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > top_score) {
            top_score = result.classification[i].value;
            top_label = result.classification[i].label;
        }
    }

    Serial.printf("%-30s -> [%-8s] %.1f%% (DSP: %dms, NN: %dms)\n",
                  file_path, top_label, top_score * 100.0f,
                  result.timing.dsp, result.timing.classification);
}

void classify_directory(const char* dirname) {
    Serial.printf("\n--- Processing ESP32 LittleFS Directory: %s ---\n", dirname);
    File root = LittleFS.open(dirname);
    if (!root || !root.isDirectory()) {
        Serial.println("Error: Failed to open directory");
        return;
    }

    int count = 0;
    File file = root.openNextFile();
    while (file) {
        String filename = String(file.name());
        if (!file.isDirectory() && filename.endsWith(".wav")) {
            String full_path = "/" + filename;
            file.close(); // Close handle before read_wav_file opens it
            classify_file(full_path.c_str());
            count++;
        } else {
            file.close();
        }
        file = root.openNextFile();
    }
    Serial.printf("Completed processing %d files on ESP32-S3.\n", count);
}

void setup() {
    Serial.begin(115200);
    delay(2000); // Allow time to open Serial Monitor
    Serial.println("\n=== ESP32-S3-N16R8 WAV Batch Classifier ===");

    // Allocate reusable float buffer in 8MB PSRAM
    psram_audio_buffer = (float*)ps_malloc(EI_CLASSIFIER_RAW_SAMPLE_COUNT * 2 * sizeof(float));
    if (!psram_audio_buffer) {
        psram_audio_buffer = (float*)malloc(EI_CLASSIFIER_RAW_SAMPLE_COUNT * sizeof(float));
    }

    if (!LittleFS.begin(true)) {
        Serial.println("Error: Failed to mount LittleFS!");
        return;
    }

    classify_directory("/");
}

void loop() {
    // Press 'r' in the Serial Monitor to re-run classification at any time
    if (Serial.available() && Serial.read() == 'r') {
        classify_directory("/");
    }
    delay(100);
}
