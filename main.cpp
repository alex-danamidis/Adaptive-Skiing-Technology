#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include "EdgeImpulse-AI-Model/edge-impulse-sdk/classifier/ei_run_classifier.h"

using namespace std;

#pragma pack(push, 1)
struct WavHeader{
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

struct AudioBufferSignal{
    const float* buffer;
    size_t total_samples;
};

int raw_audio_signal_get_data(size_t offset, size_t length, float *out_ptr){
    extern AudioBufferSignal g_audio_signal;
    for(size_t i = 0; i < length; i++){
        if((offset + i) < g_audio_signal.total_samples){
            out_ptr[i] = g_audio_signal.buffer[offset + i];
        }
        else{
            out_ptr[i] = 0.0f;
        }
    }
    return EIDSP_OK;
}

AudioBufferSignal g_audio_signal = {nullptr, 0};

bool read_wav_file(const string& path, vector<float>& pcm_floats){
    ifstream wav(path, ifstream::binary);
    if(!wav.is_open()){
        cerr << "Error: Could not open file: " << path << endl;
        return false;
    }

    WavHeader header;
    wav.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
    if(!wav){
        cerr << "Error: File too short to contain a valid WAV header.\n";
        return false;
    }

    if(strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0){
        cerr << "Error: Not a valid RIFF/WAVE file.\n";
        return false;
    }

    if(header.channels != 1 || header.bits_per_sample != 16){
        cerr << "Error: Only 16-bit mono PCM WAV files are supported (found " << header.channels << " channels, " << header.bits_per_sample << " bits).\n";
        return false;
    }

    if(header.sample_rate != EI_CLASSIFIER_FREQUENCY){
        cerr << "Warning: Expected sample rate " << EI_CLASSIFIER_FREQUENCY << " Hz, but file is " << header.sample_rate << " Hz.\n";
    }

    size_t num_samples = header.data_size / sizeof(int16_t);
    vector<int16_t> raw_samples(num_samples);
    wav.read(reinterpret_cast<char*>(raw_samples.data()), header.data_size);

    pcm_floats.resize(num_samples);
    for(size_t i = 0; i < num_samples; i++){
        pcm_floats[i] = static_cast<float>(raw_samples[i]);
    }

    cout << "Loaded " << num_samples << " audio samples (" << (float)num_samples / header.sample_rate << "s) from " << path << "\n";
    return true;
}


int main(int argc, char **argv) {
    string audio_file_path = "";

    for(int i = 1; i < argc; i++){
        if((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--file") == 0)){
            audio_file_path = argv[++i];
        }
    }

    if(audio_file_path.empty()){
        cerr << "Usage: " << argv[0] << " --file <path_to_wav_file>\n";
        cerr << "Example: " << argv[0] << " --file samples/stop_01.wav\n";
        return 1;
    }

    vector<float> audio_buffer;
    if(!read_wav_file(audio_file_path, audio_buffer)){
        return 1;
    }

    g_audio_signal.buffer = audio_buffer.data();
    g_audio_signal.total_samples = audio_buffer.size();


    signal_t signal;            // Wrapper for raw input buffer
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &raw_audio_signal_get_data;
    ei_impulse_result_t result = { 0 }; // Used to store inference output
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);       // Return code from inference
    if(res != EI_IMPULSE_OK){
        cerr << "Error: run_classifier failed with code: " << res << "\n";
        return 1;
    }

    // Print return code and how long it took to perform inference
    cout << "\n--- Classification Results ---\n";
    cout << "Timing: DSP " << result.timing.dsp << "ms, inference " << result.timing.classification << " ms, anomaly " << result.timing.anomaly << "ms\n\n";
    string top_label = "";
    float top_score = 0.0f;

    for(size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++){
        cout << " " << result.classification[i].label << ": " << result.classification[i].value << "\n";
        if(result.classification[i].value > top_score){
            top_score = result.classification[i].value;
            top_label = result.classification[i].label;
        }
    }
    cout << "\nPredicted Command: [" << top_label << "] (confidence: " << (top_score * 100.0f) << "%)\n";
    return 0;
}
