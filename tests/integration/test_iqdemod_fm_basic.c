/*
 * IQ Lab - Basic FM Demodulator Integration Test
 * Tests basic FM demodulation functionality
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
#define TEST_FREQUENCY 100000.0f
#define TEST_AUDIO_RATE 48000

// Generate synthetic FM signal
bool generate_fm_test_signal(const char *filename, float sample_rate, float duration_sec,
                           float carrier_freq, float modulation_freq, float deviation) {
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

    // Generate FM signal
    for (size_t i = 0; i < num_samples; i++) {
        float t = i / sample_rate;

        // Simple FM modulation: carrier + deviation * sin(2π * modulation_freq * t)
        float phase = 2.0f * M_PI * carrier_freq * t +
                     deviation / modulation_freq * sinf(2.0f * M_PI * modulation_freq * t);

        iq_data.data[i * 2] = cosf(phase);     // I
        iq_data.data[i * 2 + 1] = sinf(phase); // Q
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

// Test basic FM demodulation
bool test_basic_fm_demodulation() {
    printf("Testing basic FM demodulation...\n");

    const char *input_file = "test_fm_input.iq";
    const char *output_file = "test_fm_output.wav";

    // Generate test FM signal
    if (!generate_fm_test_signal(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                               TEST_FREQUENCY, 1000.0f, 50000.0f)) {
        printf("  ❌ Failed to generate test signal\n");
        return false;
    }
    printf("  ✓ Generated test FM signal: %s\n", input_file);

    // Clean up any existing output file
    remove(output_file);

    // Run FM demodulator
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-fm.exe --in %s --format s8 --rate %.0f --out %s --verbose",
            input_file, TEST_SAMPLE_RATE, output_file);

    int result = run_command(cmd);
    if (result != 0) {
        printf("  ❌ FM demodulator failed (exit code: %d)\n", result);
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

    printf("  ✓ Basic FM demodulation test passed\n");
    return true;
}

// Test FM demodulation with AGC
bool test_fm_demodulation_with_agc() {
    printf("Testing FM demodulation with AGC...\n");

    const char *input_file = "test_fm_agc_input.iq";
    const char *output_file = "test_fm_agc_output.wav";

    // Generate test FM signal
    if (!generate_fm_test_signal(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                               TEST_FREQUENCY, 1000.0f, 50000.0f)) {
        printf("  ❌ Failed to generate test signal\n");
        return false;
    }
    printf("  ✓ Generated test FM signal: %s\n", input_file);

    // Clean up any existing output file
    remove(output_file);

    // Run FM demodulator with AGC
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-fm.exe --in %s --format s8 --rate %.0f --out %s --agc --verbose",
            input_file, TEST_SAMPLE_RATE, output_file);

    int result = run_command(cmd);
    if (result != 0) {
        printf("  ❌ FM demodulator with AGC failed (exit code: %d)\n", result);
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

    printf("  ✓ FM demodulation with AGC test passed\n");
    return true;
}

// Test FM demodulation with stereo detection
bool test_fm_demodulation_with_stereo() {
    printf("Testing FM demodulation with stereo detection...\n");

    const char *input_file = "test_fm_stereo_input.iq";
    const char *output_file = "test_fm_stereo_output.wav";

    // Generate test FM signal
    if (!generate_fm_test_signal(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                               TEST_FREQUENCY, 1000.0f, 50000.0f)) {
        printf("  ❌ Failed to generate test signal\n");
        return false;
    }
    printf("  ✓ Generated test FM signal: %s\n", input_file);

    // Clean up any existing output file
    remove(output_file);

    // Run FM demodulator with stereo detection
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-fm.exe --in %s --format s8 --rate %.0f --out %s --stereo --verbose",
            input_file, TEST_SAMPLE_RATE, output_file);

    int result = run_command(cmd);
    if (result != 0) {
        printf("  ❌ FM demodulator with stereo failed (exit code: %d)\n", result);
        remove(input_file);
        return false;
    }

    // Check if output file was created
    if (!file_exists(output_file)) {
        printf("  ❌ Output file not created: %s\n", output_file);
        remove(input_file);
        return false;
    }
    printf("  ✓ Output WAV file created with stereo detection: %s\n", output_file);

    // Clean up
    remove(input_file);
    remove(output_file);

    printf("  ✓ FM demodulation with stereo test passed\n");
    return true;
}

int main() {
    printf("=====================================\n");
    printf("IQ Lab - FM Demodulator Integration Tests\n");
    printf("=====================================\n\n");

    int tests_run = 0;
    int tests_passed = 0;

    // Test basic FM demodulation
    tests_run++;
    if (test_basic_fm_demodulation()) {
        tests_passed++;
    }

    // Test FM demodulation with AGC
    tests_run++;
    if (test_fm_demodulation_with_agc()) {
        tests_passed++;
    }

    // Test FM demodulation with stereo detection
    tests_run++;
    if (test_fm_demodulation_with_stereo()) {
        tests_passed++;
    }

    printf("\n=====================================\n");
    printf("Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("=====================================\n");

    return (tests_run == tests_passed) ? EXIT_SUCCESS : EXIT_FAILURE;
}
