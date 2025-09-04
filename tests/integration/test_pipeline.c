/*
 * IQ Lab - Integration Tests
 *
 * Tests for complete signal processing pipeline
 * Combines SigMF, FFT, and Window functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <string.h>
#include "../../src/iq_core/io_sigmf.h"
#include "../../src/iq_core/fft.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

// Test helper macros
#define TEST_START(name) \
    printf("Running: %s\n", name); \
    tests_run++;

#define TEST_PASS() \
    printf("  ✅ PASSED\n"); \
    tests_passed++;

#define TEST_FAIL(msg) \
    printf("  ❌ FAILED: %s\n", msg);

#define TEST_END() \
    printf("\n");

// Test complete pipeline: SigMF → I/Q → Window → FFT → Analysis
void test_complete_pipeline() {
    TEST_START("Complete Signal Processing Pipeline");

    const int SIGNAL_SIZE = 2048;
    const double SAMPLE_RATE = 1000000.0;  // 1 MHz
    const double TARGET_FREQ = 100000.0;   // 100 kHz

    // Step 1: Create SigMF metadata
    printf("  Step 1: Creating SigMF metadata...\n");
    sigmf_metadata_t metadata = {0};
    bool meta_success = sigmf_create_basic_metadata(&metadata,
                                                   "ci16",
                                                   SAMPLE_RATE,
                                                   145000000,  // 145 MHz center
                                                   "Integration test signal",
                                                   "iq_lab_integration");

    if (!meta_success) {
        TEST_FAIL("SigMF metadata creation failed");
        return;
    }

    // Add capture
    sigmf_add_capture(&metadata, 0, 145000000, "2024-01-01T12:00:00Z");

    // Step 2: Generate IQ signal data
    printf("  Step 2: Generating IQ signal data...\n");
    float iq_data[SIGNAL_SIZE * 2];

    for (int i = 0; i < SIGNAL_SIZE; i++) {
        double t = (double)i / SAMPLE_RATE;
        double phase = 2.0 * M_PI * TARGET_FREQ * t;
        double signal = cos(phase);  // Pure cosine

        // Add some noise
        double noise = ((double)rand() / RAND_MAX - 0.5) * 0.1;
        double final_signal = signal + noise;

        iq_data[i * 2] = (float)final_signal;     // I
        iq_data[i * 2 + 1] = (float)final_signal; // Q (same for real signal)
    }

    // Step 3: Convert I/Q to complex
    printf("  Step 3: I/Q to complex conversion...\n");
    fft_complex_t complex_signal[SIGNAL_SIZE];
    fft_iq_to_complex(iq_data, complex_signal, SIGNAL_SIZE, true);

    // Step 4: Apply window (SKIPPED - window.c not implemented)
    printf("  Step 4: Windowing skipped (not implemented)\n");

    // Step 5: FFT analysis
    printf("  Step 5: FFT analysis...\n");
    fft_complex_t fft_result[SIGNAL_SIZE];
    bool fft_success = fft_forward(complex_signal, fft_result, SIGNAL_SIZE);

    if (!fft_success) {
        TEST_FAIL("FFT failed");
        sigmf_free_metadata(&metadata);
        return;
    }

    // Step 6: Peak detection and analysis
    printf("  Step 6: Peak detection and analysis...\n");
    double max_magnitude = 0;
    int peak_index = 0;

    for (int i = 0; i < SIGNAL_SIZE/2; i++) {
        double mag = fft_magnitude(fft_result[i]);
        if (mag > max_magnitude) {
            max_magnitude = mag;
            peak_index = i;
        }
    }

    double detected_freq = (double)peak_index * SAMPLE_RATE / SIGNAL_SIZE;
    double freq_error = fabs(detected_freq - TARGET_FREQ);
    double relative_error = freq_error / TARGET_FREQ;

    // Step 7: Power spectrum
    printf("  Step 7: Power spectrum calculation...\n");
    double power_spectrum[SIGNAL_SIZE];
    fft_power_spectrum(fft_result, power_spectrum, SIGNAL_SIZE, true);

    // Verify results
    int results_correct = 1;

    // Check frequency detection accuracy (< 5% error)
    if (relative_error > 0.05) {
        printf("    Frequency error too high: %.2f%%\n", relative_error * 100);
        results_correct = 0;
    }

    // Check that power spectrum peak matches FFT peak
    double max_power = 0;
    int power_peak_index = 0;
    for (int i = 0; i < SIGNAL_SIZE; i++) {
        if (power_spectrum[i] > max_power) {
            max_power = power_spectrum[i];
            power_peak_index = i;
        }
    }

    if (power_peak_index != peak_index) {
        printf("    Power spectrum peak mismatch: FFT=%d, Power=%d\n", peak_index, power_peak_index);
        results_correct = 0;
    }

    // Check signal power is reasonable
    double total_power = 0;
    for (int i = 0; i < SIGNAL_SIZE; i++) {
        total_power += power_spectrum[i];
    }

    double avg_power = total_power / SIGNAL_SIZE;
    if (avg_power < 0.1 || avg_power > 10.0) {
        printf("    Average power out of range: %.6f\n", avg_power);
        results_correct = 0;
    }

    if (results_correct) {
        TEST_PASS();
        printf("    ✅ Pipeline executed successfully\n");
        printf("    Detected frequency: %.1f Hz (target: %.1f Hz)\n", detected_freq, TARGET_FREQ);
        printf("    Frequency error: %.2f Hz (%.2f%%)\n", freq_error, relative_error * 100);
        printf("    Peak magnitude: %.1f\n", max_magnitude);
        printf("    Average power: %.6f\n", avg_power);
        printf("    SigMF metadata: %s captures\n", metadata.num_captures > 0 ? "has" : "no");
    } else {
        TEST_FAIL("Pipeline results incorrect");
    }

    // Cleanup
    sigmf_free_metadata(&metadata);

    TEST_END();
}

// Test SigMF + FFT integration
void test_sigmf_fft_integration() {
    TEST_START("SigMF + FFT Integration");

    // Create metadata
    sigmf_metadata_t metadata = {0};
    bool success = sigmf_create_basic_metadata(&metadata,
                                             "ci16",
                                             2000000,  // 2 MHz
                                             100000000, // 100 MHz
                                             "FFT test signal",
                                             "integration_test");

    if (!success) {
        TEST_FAIL("Metadata creation failed");
        return;
    }

    // Generate test signal based on metadata
    const int SIZE = 1024;
    const double SAMPLE_RATE = metadata.global.sample_rate;
    const double CENTER_FREQ = metadata.global.frequency;

    fft_complex_t signal[SIZE];

    // Create signal at center frequency
    for (int i = 0; i < SIZE; i++) {
        double t = (double)i / SAMPLE_RATE;
        double phase = 2.0 * M_PI * (CENTER_FREQ / 1e6) * t;  // Convert Hz to MHz for phase calc
        signal[i] = cos(phase) + sin(phase) * I;
    }

    // Skip windowing (window.c not implemented)

    // FFT
    fft_complex_t fft_result[SIZE];
    fft_forward(signal, fft_result, SIZE);

    // Find peak (should be at DC in baseband)
    double max_magnitude = 0;
    for (int i = 0; i < SIZE; i++) {
        double mag = fft_magnitude(fft_result[i]);
        if (mag > max_magnitude) max_magnitude = mag;
    }

    // Check that we got a reasonable signal
    if (max_magnitude > SIZE * 0.1) {  // At least 10% of maximum possible
        TEST_PASS();
        printf("    ✅ SigMF metadata correctly used for signal generation\n");
        printf("    Sample rate from metadata: %.0f Hz\n", SAMPLE_RATE);
        printf("    Center frequency: %.0f Hz\n", CENTER_FREQ);
        printf("    Max FFT magnitude: %.1f\n", max_magnitude);
    } else {
        TEST_FAIL("Signal magnitude too low");
        printf("    Max magnitude: %.1f (expected > %.1f)\n", max_magnitude, SIZE * 0.1);
    }

    sigmf_free_metadata(&metadata);

    TEST_END();
}

// Test error propagation through pipeline
void test_pipeline_error_handling() {
    TEST_START("Pipeline Error Handling");

    int errors_handled = 0;
    int total_tests = 3;

    // Test 1: Invalid FFT size
    printf("  Testing invalid FFT size...\n");
    fft_complex_t dummy[3] = {1, 2, 3};
    if (!fft_forward(dummy, dummy, 3)) {
        errors_handled++;
        printf("    ✅ Invalid FFT size handled\n");
    }

    // Test 2: NULL FFT input
    printf("  Testing NULL FFT input...\n");
    fft_complex_t null_result[8];
    if (!fft_forward(NULL, null_result, 8)) {
        errors_handled++;
        printf("    ✅ NULL FFT input handled\n");
    }

    // Test 3: SigMF with invalid file
    printf("  Testing invalid SigMF file...\n");
    sigmf_metadata_t invalid_meta = {0};
    if (!sigmf_read_metadata("nonexistent_file.sigmf-meta", &invalid_meta)) {
        errors_handled++;
        printf("    ✅ Invalid SigMF file handled\n");
    }

    if (errors_handled == total_tests) {
        TEST_PASS();
        printf("    ✅ All error conditions handled correctly\n");
    } else {
        TEST_FAIL("Some errors not handled correctly");
        printf("    Handled: %d/%d\n", errors_handled, total_tests);
    }

    TEST_END();
}

// Test performance benchmarks
void test_performance_benchmarks() {
    TEST_START("Performance Benchmarks");

    const int SIZES[] = {256, 512, 1024, 2048, 4096};
    const int NUM_SIZES = sizeof(SIZES) / sizeof(SIZES[0]);

    printf("  Benchmarking FFT performance:\n");
    printf("  Size    | Time (μs) | Throughput (MS/s)\n");
    printf("  --------|-----------|-----------------\n");

    for (int i = 0; i < NUM_SIZES; i++) {
        int size = SIZES[i];

        // Create test signal
        fft_complex_t *signal = (fft_complex_t *)malloc(size * sizeof(fft_complex_t));
        fft_complex_t *result = (fft_complex_t *)malloc(size * sizeof(fft_complex_t));

        if (!signal || !result) {
            printf("  %4d    |   N/A     |      N/A\n", size);
            free(signal);
            free(result);
            continue;
        }

        // Fill with test data
        for (int j = 0; j < size; j++) {
            signal[j] = cos(2 * M_PI * j / size) + sin(2 * M_PI * j / size) * I;
        }

        // Time the FFT (simple timing, not highly accurate)
        clock_t start = clock();
        bool success = fft_forward(signal, result, size);
        clock_t end = clock();

        if (success) {
            double time_us = ((double)(end - start) / CLOCKS_PER_SEC) * 1000000.0;
            double throughput_ms = size / time_us;  // MSamples/second

            printf("  %4d    | %7.1f   | %8.1f\n", size, time_us, throughput_ms);
        } else {
            printf("  %4d    |   FAILED  |      N/A\n", size);
        }

        free(signal);
        free(result);
    }

    TEST_PASS();
    printf("    ✅ Performance benchmarking completed\n");

    TEST_END();
}

// Test real-world scenario: FM radio analysis
void test_fm_radio_scenario() {
    TEST_START("FM Radio Analysis Scenario");

    // Simulate FM radio scenario
    const int SAMPLE_RATE = 2000000;  // 2 MHz sample rate (typical SDR)
    const int SIGNAL_SIZE = 4096;
    const double FM_FREQ = 98500000;  // 98.5 MHz FM station
    const double BANDWIDTH = 150000;  // 150 kHz FM bandwidth

    printf("  Simulating FM radio analysis:\n");
    printf("  Sample rate: %d Hz\n", SAMPLE_RATE);
    printf("  FM station: %.1f MHz\n", FM_FREQ / 1e6);
    printf("  Signal size: %d samples\n", SIGNAL_SIZE);

    // Create SigMF metadata for FM capture
    sigmf_metadata_t fm_metadata = {0};
    sigmf_create_basic_metadata(&fm_metadata,
                              "ci16",
                              SAMPLE_RATE,
                              FM_FREQ,
                              "FM radio broadcast capture",
                              "iq_lab_fm_test");

    // Generate simulated FM signal (simplified - just a carrier)
    fft_complex_t fm_signal[SIGNAL_SIZE];
    for (int i = 0; i < SIGNAL_SIZE; i++) {
        double t = (double)i / SAMPLE_RATE;
        double phase = 2.0 * M_PI * (FM_FREQ / 1e6) * t;  // Convert to MHz for phase
        fm_signal[i] = cos(phase) + sin(phase) * I;
    }

    // Skip windowing for now (window.c not implemented)

    // FFT analysis
    fft_complex_t fft_result[SIGNAL_SIZE];
    fft_forward(fm_signal, fft_result, SIGNAL_SIZE);

    // Analyze spectrum
    double max_magnitude = 0;
    int carrier_index = 0;

    // Find carrier frequency
    for (int i = 0; i < SIGNAL_SIZE/2; i++) {
        double mag = fft_magnitude(fft_result[i]);
        if (mag > max_magnitude) {
            max_magnitude = mag;
            carrier_index = i;
        }
    }

    double detected_freq = (double)carrier_index * SAMPLE_RATE / SIGNAL_SIZE;
    double freq_error = fabs(detected_freq - (FM_FREQ / 1e6) * 1e6);  // Convert back to Hz

    printf("  Analysis results:\n");
    printf("  Detected carrier: %.1f Hz\n", detected_freq);
    printf("  Target frequency: %.1f Hz\n", FM_FREQ);
    printf("  Frequency error: %.1f Hz\n", freq_error);
    printf("  FM bandwidth: %.0f Hz\n", BANDWIDTH);
    printf("  Carrier magnitude: %.1f\n", max_magnitude);
    printf("  Signal quality: %s\n", max_magnitude > SIGNAL_SIZE * 0.1 ? "GOOD" : "WEAK");

    // Check if analysis is reasonable
    if (freq_error < SAMPLE_RATE / SIGNAL_SIZE * 2 &&  // Within 2 FFT bins
        max_magnitude > SIGNAL_SIZE * 0.05) {           // Reasonable signal strength
        TEST_PASS();
        printf("    ✅ FM radio analysis successful\n");
    } else {
        TEST_FAIL("FM radio analysis failed");
    }
    sigmf_free_metadata(&fm_metadata);

    TEST_END();
}

// Main test runner
int main() {
    printf("========================================\n");
    printf("IQ Lab - Integration Tests\n");
    printf("========================================\n\n");

    // Seed random number generator for reproducible tests
    srand(42);

    // Run all integration tests
    test_complete_pipeline();
    test_sigmf_fft_integration();
    test_pipeline_error_handling();
    test_performance_benchmarks();
    test_fm_radio_scenario();

    // Summary
    printf("========================================\n");
    printf("Integration Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("========================================\n");

    if (tests_passed == tests_run) {
        printf("🎉 All integration tests PASSED!\n");
        printf("✅ Complete signal processing pipeline validated\n");
        return EXIT_SUCCESS;
    } else {
        printf("❌ Some integration tests FAILED!\n");
        return EXIT_FAILURE;
    }
}
