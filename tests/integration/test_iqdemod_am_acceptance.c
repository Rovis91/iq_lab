/*
 * IQ Lab - AM Demodulator Acceptance Test
 * Tests envelope distortion requirements for AM demodulation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "../src/iq_core/io_iq.h"
#include "../src/demod/wave.h"

#define M_PI 3.14159265358979323846
#define TEST_SAMPLE_RATE 2000000.0f
#define TEST_AUDIO_RATE 48000
#define TEST_DURATION_SEC 2.0f
#define TEST_CARRIER_FREQ 100000.0f
#define TEST_MODULATION_FREQ 1000.0f
#define TEST_MODULATION_DEPTH 0.8f
#define MAX_ENVELOPE_DISTORTION_PERCENT 5.0f

// Generate synthetic AM signal with known modulation
bool generate_am_signal_with_known_modulation(const char *filename, float sample_rate, float duration_sec,
                                             float carrier_freq, float modulation_freq, float modulation_depth) {
    size_t num_samples = (size_t)(sample_rate * duration_sec);

    // Allocate IQ data
    iq_data_t iq_data;
    iq_data.sample_rate = (uint32_t)sample_rate;
    iq_data.num_samples = num_samples;
    iq_data.data = malloc(num_samples * 2 * sizeof(float));  // Interleaved I/Q

    if (!iq_data.data) {
        fprintf(stderr, "Failed to allocate IQ data\n");
        return false;
    }

    // Generate AM signal
    // AM modulation: (1 + m * cos(2π * fm * t)) * cos(2π * fc * t)
    for (size_t i = 0; i < num_samples; i++) {
        float t = i / sample_rate;

        // AM envelope (what we expect to recover)
        float envelope = 1.0f + modulation_depth * cosf(2.0f * M_PI * modulation_freq * t);

        // Carrier
        float carrier = cosf(2.0f * M_PI * carrier_freq * t);

        // AM signal
        float am_signal = envelope * carrier;

        iq_data.data[i * 2] = am_signal;     // I (real part)
        iq_data.data[i * 2 + 1] = 0.0f;     // Q (imaginary part - zero for real AM)
    }

    // Save to file (need to create a temporary file with correct format)
    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to create output file\n");
        free(iq_data.data);
        return false;
    }

    // Convert to s8 format and write
    for (size_t i = 0; i < num_samples; i++) {
        int8_t i_val = (int8_t)(iq_data.data[i * 2] * 127.0f);
        int8_t q_val = (int8_t)(iq_data.data[i * 2 + 1] * 127.0f);
        fwrite(&i_val, 1, 1, f);
        fwrite(&q_val, 1, 1, f);
    }

    fclose(f);
    free(iq_data.data);
    return true;
}

// Calculate RMS of audio signal
double calculate_rms(const float *samples, size_t num_samples) {
    double sum_squares = 0.0;
    for (size_t i = 0; i < num_samples; i++) {
        sum_squares += samples[i] * samples[i];
    }
    return sqrt(sum_squares / num_samples);
}

// Extract envelope from demodulated audio (low-pass filter to remove carrier)
void extract_envelope(const float *audio, size_t num_samples, float sample_rate,
                     float *envelope, float cutoff_freq) {
    // Simple first-order low-pass filter to extract envelope
    float alpha = cutoff_freq / (cutoff_freq + sample_rate / (2.0f * M_PI));
    float prev_envelope = 0.0f;

    for (size_t i = 0; i < num_samples; i++) {
        // Full-wave rectification
        float rectified = fabsf(audio[i]);

        // Low-pass filter
        envelope[i] = alpha * (rectified - prev_envelope) + prev_envelope;
        prev_envelope = envelope[i];
    }
}

// Calculate modulation depth from envelope
float calculate_modulation_depth(const float *envelope, size_t num_samples) {
    float max_val = 0.0f;
    float min_val = 1.0f;

    for (size_t i = 0; i < num_samples; i++) {
        if (envelope[i] > max_val) max_val = envelope[i];
        if (envelope[i] < min_val) min_val = envelope[i];
    }

    if (max_val > 0.0f) {
        float modulation = (max_val - min_val) / (max_val + min_val);
        return modulation;
    }

    return 0.0f;
}

// Test envelope distortion requirement
bool test_am_envelope_distortion() {
    printf("Testing AM demodulator envelope distortion...\n");
    printf("Target: Envelope distortion ≤ %.1f%%\n", MAX_ENVELOPE_DISTORTION_PERCENT);

    const char *input_file = "test_am_envelope_input.iq";
    const char *output_file = "test_am_envelope_output.wav";

    // Generate test AM signal with known modulation depth
    if (!generate_am_signal_with_known_modulation(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                                                 TEST_CARRIER_FREQ, TEST_MODULATION_FREQ,
                                                 TEST_MODULATION_DEPTH)) {
        printf("  ❌ Failed to generate test AM signal\n");
        return false;
    }
    printf("  ✓ Generated test AM signal with modulation depth: %.1f%%\n",
           TEST_MODULATION_DEPTH * 100.0f);

    // Clean up any existing output file
    remove(output_file);

    // Run AM demodulator
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-am.exe --in %s --format s8 --rate %.0f --out %s --verbose",
            input_file, TEST_SAMPLE_RATE, output_file);

    int result = system(cmd);
    if (result != 0) {
        printf("  ❌ AM demodulator failed (exit code: %d)\n", result);
        remove(input_file);
        return false;
    }

    // Load and analyze output WAV file
    wave_reader_t wav_reader;
    if (!wave_reader_open(&wav_reader, output_file)) {
        printf("  ❌ Failed to open output WAV file\n");
        remove(input_file);
        remove(output_file);
        return false;
    }

    // Read all audio samples
    float *audio_samples = malloc(wav_reader.data_size / 2);  // 16-bit samples
    if (!audio_samples) {
        printf("  ❌ Failed to allocate memory for audio samples\n");
        wave_reader_close(&wav_reader);
        remove(input_file);
        remove(output_file);
        return false;
    }

    size_t samples_read = wave_read_samples(&wav_reader, audio_samples,
                                          wav_reader.data_size / 2);

    if (samples_read == 0) {
        printf("  ❌ Failed to read audio samples from WAV file\n");
        free(audio_samples);
        wave_reader_close(&wav_reader);
        remove(input_file);
        remove(output_file);
        return false;
    }

    printf("  ✓ WAV format: %d Hz, %d channels, %d bits\n",
           wav_reader.sample_rate, wav_reader.channels, wav_reader.bits_per_sample);

    // Extract envelope from demodulated audio
    float *extracted_envelope = malloc(samples_read * sizeof(float));
    if (!extracted_envelope) {
        printf("  ❌ Failed to allocate memory for envelope extraction\n");
        free(audio_samples);
        wave_reader_close(&wav_reader);
        remove(input_file);
        remove(output_file);
        return false;
    }

    extract_envelope(audio_samples, samples_read, wav_reader.sample_rate,
                    extracted_envelope, TEST_MODULATION_FREQ * 2.0f);  // 2x modulation freq for cutoff

    // Calculate measured modulation depth
    float measured_modulation = calculate_modulation_depth(extracted_envelope, samples_read);
    float expected_modulation = TEST_MODULATION_DEPTH;

    printf("  ✓ Expected modulation depth: %.1f%%\n", expected_modulation * 100.0f);
    printf("  ✓ Measured modulation depth: %.1f%%\n", measured_modulation * 100.0f);

    // Calculate distortion
    float distortion_percent = fabsf(measured_modulation - expected_modulation) / expected_modulation * 100.0f;
    printf("  ✓ Envelope distortion: %.2f%%\n", distortion_percent);

    // Clean up
    free(audio_samples);
    free(extracted_envelope);
    wave_reader_close(&wav_reader);
    remove(input_file);
    remove(output_file);

    // Check distortion requirement
    if (distortion_percent <= MAX_ENVELOPE_DISTORTION_PERCENT) {
        printf("  ✅ Envelope distortion requirement met: %.2f%% ≤ %.1f%%\n",
               distortion_percent, MAX_ENVELOPE_DISTORTION_PERCENT);
        return true;
    } else {
        printf("  ❌ Envelope distortion requirement NOT met: %.2f%% > %.1f%%\n",
               distortion_percent, MAX_ENVELOPE_DISTORTION_PERCENT);
        return false;
    }
}

int main() {
    printf("=====================================\n");
    printf("IQ Lab - AM Demodulator Acceptance Test\n");
    printf("=====================================\n\n");

    int tests_run = 0;
    int tests_passed = 0;

    // Test envelope distortion requirement
    tests_run++;
    if (test_am_envelope_distortion()) {
        tests_passed++;
    }

    printf("\n=====================================\n");
    printf("Acceptance Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("=====================================\n");

    // Acceptance criteria summary
    if (tests_passed == tests_run) {
        printf("🎉 AM Demodulator ACCEPTANCE CRITERIA MET!\n");
        printf("   • Envelope distortion ≤ %.1f%% ✓\n", MAX_ENVELOPE_DISTORTION_PERCENT);
    } else {
        printf("❌ AM Demodulator acceptance criteria NOT fully met\n");
    }

    return (tests_run == tests_passed) ? EXIT_SUCCESS : EXIT_FAILURE;
}
