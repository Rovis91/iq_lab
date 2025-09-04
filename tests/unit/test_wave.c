/*
 * IQ Lab - WAV Writer Unit Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "../../src/demod/wave.h"

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_START(name) \
    printf("Running: %s\n", name); \
    tests_run++;

#define TEST_PASS() \
    printf("  ✅ PASSED\n"); \
    tests_passed++;

#define TEST_FAIL(msg) \
    printf("  ❌ FAILED: %s\n", msg);

// Test basic WAV writer functionality
void test_wave_writer_basic() {
    TEST_START("Basic WAV Writer");

    const char *test_file = "test_output.wav";
    wave_writer_t writer;

    // Initialize writer (mono, 48kHz)
    if (!wave_writer_init(&writer, test_file, 48000, 1)) {
        TEST_FAIL("Failed to initialize writer");
        return;
    }

    // Generate test samples (1kHz sine wave)
    const int num_samples = 4800; // 0.1 seconds
    int16_t *samples = malloc(num_samples * sizeof(int16_t));
    if (!samples) {
        TEST_FAIL("Failed to allocate samples");
        wave_writer_close(&writer);
        return;
    }

    for (int i = 0; i < num_samples; i++) {
        // 1kHz sine wave at 0dBFS
        double phase = 2.0 * 3.141592653589793 * 1000.0 * i / 48000.0;
        samples[i] = (int16_t)(sin(phase) * 32767.0);
    }

    // Write samples
    if (!wave_writer_write_samples(&writer, samples, num_samples)) {
        TEST_FAIL("Failed to write samples");
        free(samples);
        wave_writer_close(&writer);
        return;
    }

    // Close writer
    if (!wave_writer_close(&writer)) {
        TEST_FAIL("Failed to close writer");
        free(samples);
        return;
    }

    // Verify file was created and has reasonable size
    FILE *f = fopen(test_file, "rb");
    if (!f) {
        TEST_FAIL("Output file not created");
        free(samples);
        return;
    }

    // Check file size (should be around 44 + 2*4800 = ~9644 bytes)
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fclose(f);

    if (file_size < 9000 || file_size > 11000) {
        TEST_FAIL("File size incorrect");
        printf("    Expected: ~9644 bytes, Got: %ld bytes\n", file_size);
        free(samples);
        remove(test_file);
        return;
    }

    // Cleanup
    free(samples);
    remove(test_file);
    TEST_PASS();
}

// Test stereo WAV writer
void test_wave_writer_stereo() {
    TEST_START("Stereo WAV Writer");

    const char *test_file = "test_stereo.wav";
    wave_writer_t writer;

    // Initialize writer (stereo, 44.1kHz)
    if (!wave_writer_init(&writer, test_file, 44100, 2)) {
        TEST_FAIL("Failed to initialize stereo writer");
        return;
    }

    // Generate test samples (interleaved stereo)
    const int num_samples = 4410; // 0.1 seconds per channel
    int16_t *samples = malloc(num_samples * 2 * sizeof(int16_t)); // 2 channels
    if (!samples) {
        TEST_FAIL("Failed to allocate stereo samples");
        wave_writer_close(&writer);
        return;
    }

    for (int i = 0; i < num_samples; i++) {
        // Left channel: 1kHz sine
        double phase_left = 2.0 * 3.141592653589793 * 1000.0 * i / 44100.0;
        samples[i * 2] = (int16_t)(sin(phase_left) * 32767.0);

        // Right channel: 2kHz sine
        double phase_right = 2.0 * 3.141592653589793 * 2000.0 * i / 44100.0;
        samples[i * 2 + 1] = (int16_t)(sin(phase_right) * 32767.0);
    }

    // Write samples
    if (!wave_writer_write_samples(&writer, samples, num_samples)) {
        TEST_FAIL("Failed to write stereo samples");
        free(samples);
        wave_writer_close(&writer);
        return;
    }

    // Close writer
    if (!wave_writer_close(&writer)) {
        TEST_FAIL("Failed to close stereo writer");
        free(samples);
        return;
    }

    // Verify file exists
    FILE *f = fopen(test_file, "rb");
    if (!f) {
        TEST_FAIL("Stereo output file not created");
        free(samples);
        return;
    }
    fclose(f);

    // Cleanup
    free(samples);
    remove(test_file);
    TEST_PASS();
}

// Test error conditions
void test_wave_writer_errors() {
    TEST_START("Error Conditions");

    wave_writer_t writer;
    int errors_handled = 0;

    // Test NULL filename
    if (!wave_writer_init(&writer, NULL, 48000, 1)) {
        errors_handled++;
    }

    // Test invalid channels
    if (!wave_writer_init(&writer, "test.wav", 48000, 0)) {
        errors_handled++;
    }

    // Test invalid sample rate
    if (!wave_writer_init(&writer, "test.wav", 0, 1)) {
        errors_handled++;
    }

    if (errors_handled == 3) {
        TEST_PASS();
    } else {
        TEST_FAIL("Not all error conditions handled");
        printf("    Handled: %d/3 errors\n", errors_handled);
    }
}

int main() {
    printf("=====================================\n");
    printf("IQ Lab - WAV Writer Unit Tests\n");
    printf("=====================================\n\n");

    test_wave_writer_basic();
    test_wave_writer_stereo();
    test_wave_writer_errors();

    printf("\n=====================================\n");
    printf("Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("=====================================\n");

    return tests_run == tests_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
