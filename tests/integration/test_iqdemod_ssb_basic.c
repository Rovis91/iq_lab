/*
 * IQ Lab - Basic SSB Demodulator Integration Test
 * Tests basic SSB demodulation functionality
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
#define TEST_BFO_FREQ 1500.0f
#define TEST_AUDIO_RATE 48000

// Generate synthetic SSB signal
bool generate_ssb_test_signal(const char *filename, float sample_rate, float duration_sec,
                            float carrier_freq, float modulation_freq, float bfo_freq,
                            bool is_usb) {
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

    // Generate SSB signal
    // SSB modulation: cos(2π(fc ± f_mod)t) * cos(2π*fm*t)
    // where + is for USB, - is for LSB
    float sideband_offset = is_usb ? bfo_freq : -bfo_freq;

    for (size_t i = 0; i < num_samples; i++) {
        float t = i / sample_rate;

        // SSB carrier frequency (fc ± bfo_freq)
        float ssb_freq = carrier_freq + sideband_offset;

        // Modulated SSB signal
        float ssb_signal = cosf(2.0f * M_PI * ssb_freq * t) * cosf(2.0f * M_PI * modulation_freq * t);

        iq_data.data[i * 2] = ssb_signal;     // I (real part)
        iq_data.data[i * 2 + 1] = 0.0f;      // Q (imaginary part - zero for real SSB)
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

// Test basic USB SSB demodulation
bool test_basic_ssb_usb() {
    printf("Testing basic SSB USB demodulation...\n");

    const char *input_file = "test_ssb_usb_input.iq";
    const char *output_file = "test_ssb_usb_output.wav";

    // Generate test USB SSB signal
    if (!generate_ssb_test_signal(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                               TEST_CARRIER_FREQ, TEST_MODULATION_FREQ, TEST_BFO_FREQ, true)) {
        printf("  ❌ Failed to generate test USB SSB signal\n");
        return false;
    }
    printf("  ✓ Generated test USB SSB signal: %s\n", input_file);

    // Clean up any existing output file
    remove(output_file);

    // Run SSB demodulator
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-ssb.exe --in %s --format s8 --rate %.0f --out %s --mode usb --bfo %.0f --verbose",
            input_file, TEST_SAMPLE_RATE, output_file, TEST_BFO_FREQ);

    int result = run_command(cmd);
    if (result != 0) {
        printf("  ❌ SSB USB demodulator failed (exit code: %d)\n", result);
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

    printf("  ✓ Basic SSB USB demodulation test passed\n");
    return true;
}

// Test basic LSB SSB demodulation
bool test_basic_ssb_lsb() {
    printf("Testing basic SSB LSB demodulation...\n");

    const char *input_file = "test_ssb_lsb_input.iq";
    const char *output_file = "test_ssb_lsb_output.wav";

    // Generate test LSB SSB signal
    if (!generate_ssb_test_signal(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                               TEST_CARRIER_FREQ, TEST_MODULATION_FREQ, TEST_BFO_FREQ, false)) {
        printf("  ❌ Failed to generate test LSB SSB signal\n");
        return false;
    }
    printf("  ✓ Generated test LSB SSB signal: %s\n", input_file);

    // Clean up any existing output file
    remove(output_file);

    // Run SSB demodulator
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-ssb.exe --in %s --format s8 --rate %.0f --out %s --mode lsb --bfo %.0f --verbose",
            input_file, TEST_SAMPLE_RATE, output_file, TEST_BFO_FREQ);

    int result = run_command(cmd);
    if (result != 0) {
        printf("  ❌ SSB LSB demodulator failed (exit code: %d)\n", result);
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

    // Clean up
    remove(input_file);
    remove(output_file);

    printf("  ✓ Basic SSB LSB demodulation test passed\n");
    return true;
}

// Test SSB demodulation with AGC
bool test_ssb_demodulation_with_agc() {
    printf("Testing SSB demodulation with AGC...\n");

    const char *input_file = "test_ssb_agc_input.iq";
    const char *output_file = "test_ssb_agc_output.wav";

    // Generate test USB SSB signal
    if (!generate_ssb_test_signal(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                               TEST_CARRIER_FREQ, TEST_MODULATION_FREQ, TEST_BFO_FREQ, true)) {
        printf("  ❌ Failed to generate test SSB signal\n");
        return false;
    }
    printf("  ✓ Generated test SSB signal: %s\n", input_file);

    // Clean up any existing output file
    remove(output_file);

    // Run SSB demodulator with AGC
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-ssb.exe --in %s --format s8 --rate %.0f --out %s --mode usb --bfo %.0f --agc --verbose",
            input_file, TEST_SAMPLE_RATE, output_file, TEST_BFO_FREQ);

    int result = run_command(cmd);
    if (result != 0) {
        printf("  ❌ SSB demodulator with AGC failed (exit code: %d)\n", result);
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

    printf("  ✓ SSB demodulation with AGC test passed\n");
    return true;
}

// Test SSB demodulation with custom LPF cutoff
bool test_ssb_demodulation_custom_lpf() {
    printf("Testing SSB demodulation with custom LPF cutoff...\n");

    const char *input_file = "test_ssb_lpf_input.iq";
    const char *output_file = "test_ssb_lpf_output.wav";

    // Generate test USB SSB signal
    if (!generate_ssb_test_signal(input_file, TEST_SAMPLE_RATE, TEST_DURATION_SEC,
                               TEST_CARRIER_FREQ, TEST_MODULATION_FREQ, TEST_BFO_FREQ, true)) {
        printf("  ❌ Failed to generate test SSB signal\n");
        return false;
    }
    printf("  ✓ Generated test SSB signal: %s\n", input_file);

    // Clean up any existing output file
    remove(output_file);

    // Run SSB demodulator with custom LPF
    char cmd[512];
    sprintf(cmd, ".\\iqdemod-ssb.exe --in %s --format s8 --rate %.0f --out %s --mode usb --bfo %.0f --lpf-cutoff 2500 --verbose",
            input_file, TEST_SAMPLE_RATE, output_file, TEST_BFO_FREQ);

    int result = run_command(cmd);
    if (result != 0) {
        printf("  ❌ SSB demodulator with custom LPF failed (exit code: %d)\n", result);
        remove(input_file);
        return false;
    }

    // Check if output file was created
    if (!file_exists(output_file)) {
        printf("  ❌ Output file not created: %s\n", output_file);
        remove(input_file);
        return false;
    }
    printf("  ✓ Output WAV file created with custom LPF: %s\n", output_file);

    // Clean up
    remove(input_file);
    remove(output_file);

    printf("  ✓ SSB demodulation with custom LPF test passed\n");
    return true;
}

int main() {
    printf("=====================================\n");
    printf("IQ Lab - SSB Demodulator Integration Tests\n");
    printf("=====================================\n\n");

    int tests_run = 0;
    int tests_passed = 0;

    // Test basic SSB USB demodulation
    tests_run++;
    if (test_basic_ssb_usb()) {
        tests_passed++;
    }

    // Test basic SSB LSB demodulation
    tests_run++;
    if (test_basic_ssb_lsb()) {
        tests_passed++;
    }

    // Test SSB demodulation with AGC
    tests_run++;
    if (test_ssb_demodulation_with_agc()) {
        tests_passed++;
    }

    // Test SSB demodulation with custom LPF
    tests_run++;
    if (test_ssb_demodulation_custom_lpf()) {
        tests_passed++;
    }

    printf("\n=====================================\n");
    printf("Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("=====================================\n");

    return (tests_run == tests_passed) ? EXIT_SUCCESS : EXIT_FAILURE;
}
