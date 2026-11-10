#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

#include "commands.h"

using namespace std;
namespace fs = std::filesystem;

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

void classify_file(const string& file_path, bool verbose=false){
    vector<float> audio_buffer;
    if(!read_wav_file(file_path, audio_buffer)){
        cerr << "Skipping file: " << file_path << "\n";
        return;
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
        return;
    }

    // converts results to a vector
    std::vector<CommandPrediction> scores;

    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        CommandPrediction prediction;
        prediction.label = result.classification[i].label;
        prediction.confidence = result.classification[i].value;
        scores.push_back(prediction);
    }

    // classify and validate the command
    CommandPrediction prediction = classifyCommand(scores);
    Command command = validateCommand(prediction);

    if(verbose){
        cout << "\nClassification Results: " << file_path << "\n";
        cout << "Timing: DSP " << result.timing.dsp << "ms, inference " << result.timing.classification << " ms, anomaly " << result.timing.anomaly << " ms\n\n";

        for(size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++){
            cout << " " << left << setw(12) << result.classification[i].label << ": " << fixed << setprecision(3) << result.classification[i].value << "\n";
        }
        cout << "\nPredicted Command: [" << prediction.label << "] (confidence: " << fixed << setprecision(1) << (prediction.confidence * 100.0f) << "%)\n";
    }
    else{
        cout << left << setw(35) << fs::path(file_path).filename().string() << " -> [" << setw(8) << prediction.label << "] " << fixed << setprecision(1) << (prediction.confidence * 100.0f) << "%\n";
    }

    // check if command was accepted
    if (command == Command::INVALID) {
        cout << "Command rejected.\n";
    } else {
        cout << "Command accepted.\n";
        // pass command to haptic module
    }
}

int main(int argc, char **argv) {
    string target_path = "";
    bool is_directory = false;

    for(int i = 1; i < argc; i++){
        if((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--file") == 0)){
            if(i + 1 < argc){
                target_path = argv[++i];
                is_directory = false;
            }
            else{
                cerr << "Error: " << argv[i] << " requires a file path.\n";
                return 1;
            }
        }
        else if(strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0){
            if(i + 1 < argc){
                target_path = argv[++i];
                is_directory = true;
            }
            else{
                cerr << "Error: " << argv[i] << " requires a directory path.\n";
                return 1;
            }
        }
        else if(argv[i][0] != '-'){
            target_path = argv[i];
        }
    }

    if(target_path.empty()){
        cerr << "Usage:\n"; 
        cerr << " Single file: " << argv[0] << " -f <path_to_wav_file>\n";
        cerr << " Directory: " << argv[0] << " -d <path_to_directory>\n";
        cerr << "Example:\n"; 
        cerr << " " << argv[0] << " -f samples/stop_01.wav\n";
        cerr << " " << argv[0] << " -d wavSamples/\n";
        return 1;
    }

    if(!fs::exists(target_path)){
        cerr << "Error: Path does not exist: " << target_path << "\n";
        return 1;
    }
    if(fs::is_directory(target_path)){
        is_directory = true;
    }
    if(is_directory){
        cout << "\nProcessing Directory: " << target_path << "\n";
        int count = 0;
        for(const auto& entry : fs::directory_iterator(target_path)){
            if(entry.is_regular_file() && entry.path().extension() == ".wav"){
                classify_file(entry.path().string(), false);
                count++;
            }
        }
        cout << "\nCompleted processing " << count << " files.\n";
    }
    else{
        classify_file(target_path, true);
    }
    return 0;
}