/*
 * IQ Lab - Basic AM Demodulator Integration Test
 * Tests basic AM demodulation functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "../../src/iq_core/io_iq.h"

#define M_PI 3.14159265358979323846
#define TEST_SAMPLE_RATE 2000000.0f
#define TEST_DURATION_SEC 1.0f
#define TEST_CARRIER_FREQ 100000.0f
#define TEST_MODULATION_FREQ 1000.0f
#define TEST_MODULATION_DEPTH 0.8f
#define TEST_AUDIO_RATE 48000

// Generate synthetic AM signal
bool generate_am_test_signal(const char *filename, float sample_rate, float duration_sec,
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
    // where m is modulation depth (0 to 1)
    for (size_t i = 0; i < num_samples; i++) {
        float t = i / sample_rate;

        // AM envelope
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

// Check if file exists
bool file_exists(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

// Run system command
int run_command(const char *cmd) {
    printf("Running: %s\n", cmd);
    return system(cmd);
}

// Test basic AM demodulation
bool test_basic_am_demodulation() {
    printf("Testing basic AM demodulation...\n");

    const char *input_file = "test_am_input.iq";
    const char *output_file = "test_am_output.wav";

    // Generate test AM signal
    if (!generate_am_test_signal(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                               TEST_CARRIER_FREQ, TEST_MODULATION_FREQ, TEST_MODULATION_DEPTH)) {
        printf("  ❌ Failed to generate test AM signal\n");
        return false;
    }
    printf("  ✓ Generated test AM signal: %s\n", input_file);

    // Clean up any existing output file
    remove(output_file);

    // Run AM demodulator
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-am.exe --in %s --format s8 --rate %.0f --out %s --verbose",
            input_file, TEST_SAMPLE_RATE, output_file);

    int result = run_command(cmd);
    if (result != 0) {
        printf("  ❌ AM demodulator failed (exit code: %d)\n", result);
        remove(input_file);
        return false;
    }

    // Check if output file was created
    if (!file_exists(output_file)) {
        printf("  ❌ Output file not created: %s\n", output_file);
        remove(input_file);
        return false;
    }
    printf("  ✓ Output WAV file created: %s\n", output_file);

    // Check file size (should be reasonable)
    FILE *f = fopen(output_file, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);

        // Expected size: WAV header + PCM data
        // At 48kHz, 16-bit mono, 1 second = ~96KB + header
        long expected_min = 48000 * 2;  // 48kHz * 16-bit * 1sec minimum
        if (size < expected_min) {
            printf("  ❌ Output file too small: %ld bytes (expected > %ld)\n", size, expected_min);
            remove(input_file);
            remove(output_file);
            return false;
        }
        printf("  ✓ Output file size: %ld bytes\n", size);
    }

    // Clean up
    remove(input_file);
    remove(output_file);

    printf("  ✓ Basic AM demodulation test passed\n");
    return true;
}

// Test AM demodulation with AGC
bool test_am_demodulation_with_agc() {
    printf("Testing AM demodulation with AGC...\n");

    const char *input_file = "test_am_agc_input.iq";
    const char *output_file = "test_am_agc_output.wav";

    // Generate test AM signal
    if (!generate_am_test_signal(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                               TEST_CARRIER_FREQ, TEST_MODULATION_FREQ, TEST_MODULATION_DEPTH)) {
        printf("  ❌ Failed to generate test AM signal\n");
        return false;
    }
    printf("  ✓ Generated test AM signal: %s\n", input_file);

    // Clean up any existing output file
    remove(output_file);

    // Run AM demodulator with AGC
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-am.exe --in %s --format s8 --rate %.0f --out %s --agc --verbose",
            input_file, TEST_SAMPLE_RATE, output_file);

    int result = run_command(cmd);
    if (result != 0) {
        printf("  ❌ AM demodulator with AGC failed (exit code: %d)\n", result);
        remove(input_file);
        return false;
    }

    // Check if output file was created
    if (!file_exists(output_file)) {
        printf("  ❌ Output file not created: %s\n", output_file);
        remove(input_file);
        return false;
    }
    printf("  ✓ Output WAV file created with AGC: %s\n", output_file);

    // Clean up
    remove(input_file);
    remove(output_file);

    printf("  ✓ AM demodulation with AGC test passed\n");
    return true;
}

// Test AM demodulation with custom DC blocking
bool test_am_demodulation_custom_dc() {
    printf("Testing AM demodulation with custom DC blocking...\n");

    const char *input_file = "test_am_dc_input.iq";
    const char *output_file = "test_am_dc_output.wav";

    // Generate test AM signal
    if (!generate_am_test_signal(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                               TEST_CARRIER_FREQ, TEST_MODULATION_FREQ, TEST_MODULATION_DEPTH)) {
        printf("  ❌ Failed to generate test AM signal\n");
        return false;
    }
    printf("  ✓ Generated test AM signal: %s\n", input_file);

    // Clean up any existing output file
    remove(output_file);

    // Run AM demodulator with custom DC blocking
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-am.exe --in %s --format s8 --rate %.0f --out %s --dc-cutoff 50 --verbose",
            input_file, TEST_SAMPLE_RATE, output_file);

    int result = run_command(cmd);
    if (result != 0) {
        printf("  ❌ AM demodulator with custom DC blocking failed (exit code: %d)\n", result);
        remove(input_file);
        return false;
    }

    // Check if output file was created
    if (!file_exists(output_file)) {
        printf("  ❌ Output file not created: %s\n", output_file);
        remove(input_file);
        return false;
    }
    printf("  ✓ Output WAV file created with custom DC blocking: %s\n", output_file);

    // Clean up
    remove(input_file);
    remove(output_file);

    printf("  ✓ AM demodulation with custom DC blocking test passed\n");
    return true;
}

int main() {
    printf("=====================================\n");
    printf("IQ Lab - AM Demodulator Integration Tests\n");
    printf("=====================================\n\n");

    int tests_run = 0;
    int tests_passed = 0;

    // Test basic AM demodulation
    tests_run++;
    if (test_basic_am_demodulation()) {
        tests_passed++;
    }

    // Test AM demodulation with AGC
    tests_run++;
    if (test_am_demodulation_with_agc()) {
        tests_passed++;
    }

    // Test AM demodulation with custom DC blocking
    tests_run++;
    if (test_am_demodulation_custom_dc()) {
        tests_passed++;
    }

    printf("\n=====================================\n");
    printf("Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("=====================================\n");

    return (tests_run == tests_passed) ? EXIT_SUCCESS : EXIT_FAILURE;
}
