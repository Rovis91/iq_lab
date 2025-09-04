/*
 * IQ Lab - FM Demodulator Acceptance Test
 * Tests SNR requirements: Audio SNR ≥ 25 dB @ input SNR 30 dB
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "../../src/iq_core/io_iq.h"
#include "../../src/demod/wave.h"

#define M_PI 3.14159265358979323846
#define TEST_SAMPLE_RATE 2000000.0f
#define TEST_AUDIO_RATE 48000
#define TEST_DURATION_SEC 2.0f
#define TEST_CARRIER_FREQ 100000.0f
#define TEST_MODULATION_FREQ 1000.0f
#define TARGET_INPUT_SNR_DB 30.0f
#define MIN_OUTPUT_SNR_DB 25.0f

// Generate synthetic FM signal with controlled SNR
bool generate_fm_signal_with_snr(const char *filename, float sample_rate, float duration_sec,
                                float carrier_freq, float modulation_freq, float deviation,
                                float target_snr_db) {
    size_t num_samples = (size_t)(sample_rate * duration_sec);

    // Allocate IQ data
    iq_data_t iq_data;
    iq_data.sample_rate = sample_rate;
    iq_data.num_samples = num_samples;
    iq_data.i_samples = malloc(num_samples * sizeof(float));
    iq_data.q_samples = malloc(num_samples * sizeof(float));

    if (!iq_data.i_samples || !iq_data.q_samples) {
        fprintf(stderr, "Failed to allocate IQ data\n");
        return false;
    }

    // Generate clean FM signal
    for (size_t i = 0; i < num_samples; i++) {
        float t = i / sample_rate;

        // FM modulation
        float phase = 2.0f * M_PI * carrier_freq * t +
                     deviation / modulation_freq * sinf(2.0f * M_PI * modulation_freq * t);

        iq_data.i_samples[i] = cosf(phase);
        iq_data.q_samples[i] = sinf(phase);
    }

    // Calculate signal power
    double signal_power = 0.0;
    for (size_t i = 0; i < num_samples; i++) {
        signal_power += iq_data.i_samples[i] * iq_data.i_samples[i] +
                       iq_data.q_samples[i] * iq_data.q_samples[i];
    }
    signal_power /= num_samples;

    // Calculate noise power for target SNR
    double noise_power = signal_power / pow(10.0, target_snr_db / 10.0);
    double noise_std = sqrt(noise_power);

    // Add Gaussian noise
    for (size_t i = 0; i < num_samples; i++) {
        // Simple pseudo-random noise (not perfect Gaussian, but adequate for test)
        float noise_i = noise_std * (rand() / (float)RAND_MAX - 0.5f) * 2.0f;
        float noise_q = noise_std * (rand() / (float)RAND_MAX - 0.5f) * 2.0f;

        iq_data.i_samples[i] += noise_i;
        iq_data.q_samples[i] += noise_q;
    }

    // Save to file
    if (!iq_data_save_file(&iq_data, filename, 0)) {  // 0 = s8 format
        fprintf(stderr, "Failed to save IQ data to %s\n", filename);
        free(iq_data.i_samples);
        free(iq_data.q_samples);
        return false;
    }

    free(iq_data.i_samples);
    free(iq_data.q_samples);
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

// Calculate SNR of audio signal (assuming signal is centered around 0)
double calculate_snr_db(const float *samples, size_t num_samples) {
    // Calculate RMS (signal power)
    double rms = calculate_rms(samples, num_samples);
    double signal_power = rms * rms;

    // Estimate noise power using high-frequency components or just the RMS of residual
    // For simplicity, assume the modulation is the signal and small variations are noise
    double noise_power = 0.0;
    int noise_samples = 0;

    // Use samples with small amplitude as noise estimate
    for (size_t i = 0; i < num_samples; i++) {
        if (fabsf(samples[i]) < rms * 0.1f) {  // Samples below 10% of RMS
            noise_power += samples[i] * samples[i];
            noise_samples++;
        }
    }

    if (noise_samples == 0) {
        // Fallback: use 1% of signal power as noise estimate
        noise_power = signal_power * 0.01;
    } else {
        noise_power /= noise_samples;
    }

    if (noise_power > 0.0) {
        return 10.0 * log10(signal_power / noise_power);
    }

    return 60.0;  // Very high SNR if no noise detected
}

// Test SNR requirement
bool test_fm_snr_requirement() {
    printf("Testing FM demodulator SNR requirement...\n");
    printf("Target: Audio SNR ≥ %.1f dB @ Input SNR %.1f dB\n",
           MIN_OUTPUT_SNR_DB, TARGET_INPUT_SNR_DB);

    const char *input_file = "test_fm_snr_input.iq";
    const char *output_file = "test_fm_snr_output.wav";

    // Seed random number generator for reproducible noise
    srand(42);

    // Generate test FM signal with controlled SNR
    if (!generate_fm_signal_with_snr(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                                   TEST_CARRIER_FREQ, TEST_MODULATION_FREQ, 50000.0f,
                                   TARGET_INPUT_SNR_DB)) {
        printf("  ❌ Failed to generate test signal with SNR %.1f dB\n", TARGET_INPUT_SNR_DB);
        return false;
    }
    printf("  ✓ Generated test FM signal with input SNR: %.1f dB\n", TARGET_INPUT_SNR_DB);

    // Clean up any existing output file
    remove(output_file);

    // Run FM demodulator
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-fm.exe --in %s --format s8 --rate %.0f --out %s --verbose",
            input_file, TEST_SAMPLE_RATE, output_file);

    int result = system(cmd);
    if (result != 0) {
        printf("  ❌ FM demodulator failed (exit code: %d)\n", result);
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

    // Calculate output SNR
    double output_snr_db = calculate_snr_db(audio_samples, samples_read);
    printf("  ✓ Output audio SNR: %.2f dB\n", output_snr_db);
    printf("  ✓ WAV format: %d Hz, %d channels, %d bits\n",
           wav_reader.sample_rate, wav_reader.channels, wav_reader.bits_per_sample);

    // Clean up
    free(audio_samples);
    wave_reader_close(&wav_reader);
    remove(input_file);
    remove(output_file);

    // Check SNR requirement
    if (output_snr_db >= MIN_OUTPUT_SNR_DB) {
        printf("  ✅ SNR requirement met: %.2f dB ≥ %.1f dB\n",
               output_snr_db, MIN_OUTPUT_SNR_DB);
        return true;
    } else {
        printf("  ❌ SNR requirement NOT met: %.2f dB < %.1f dB\n",
               output_snr_db, MIN_OUTPUT_SNR_DB);
        return false;
    }
}

int main() {
    printf("=====================================\n");
    printf("IQ Lab - FM Demodulator Acceptance Test\n");
    printf("=====================================\n\n");

    int tests_run = 0;
    int tests_passed = 0;

    // Test SNR requirement
    tests_run++;
    if (test_fm_snr_requirement()) {
        tests_passed++;
    }

    printf("\n=====================================\n");
    printf("Acceptance Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("=====================================\n");

    // Acceptance criteria summary
    if (tests_passed == tests_run) {
        printf("🎉 FM Demodulator ACCEPTANCE CRITERIA MET!\n");
        printf("   • Audio SNR ≥ %.1f dB @ Input SNR %.1f dB ✓\n",
               MIN_OUTPUT_SNR_DB, TARGET_INPUT_SNR_DB);
    } else {
        printf("❌ FM Demodulator acceptance criteria NOT fully met\n");
    }

    return (tests_run == tests_passed) ? EXIT_SUCCESS : EXIT_FAILURE;
}
