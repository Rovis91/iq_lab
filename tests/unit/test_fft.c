/*
 * IQ Lab - FFT Unit Tests
 *
 * Tests for Fast Fourier Transform functionality
 * Covers mathematical accuracy, edge cases, and performance
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <assert.h>
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

// Test power-of-two utilities
void test_power_of_two() {
    TEST_START("Power-of-Two Utilities");

    // Test fft_is_power_of_two
    if (fft_is_power_of_two(1) && fft_is_power_of_two(2) && fft_is_power_of_two(1024) &&
        !fft_is_power_of_two(3) && !fft_is_power_of_two(100) && !fft_is_power_of_two(1023)) {
        TEST_PASS();
    } else {
        TEST_FAIL("fft_is_power_of_two failed");
    }

    // Test fft_next_power_of_two
    if (fft_next_power_of_two(1) == 1 &&
        fft_next_power_of_two(3) == 4 &&
        fft_next_power_of_two(100) == 128 &&
        fft_next_power_of_two(1023) == 1024 &&
        fft_next_power_of_two(1024) == 1024) {
        TEST_PASS();
    } else {
        TEST_FAIL("fft_next_power_of_two failed");
    }

    TEST_END();
}

// Test FFT plan creation
void test_fft_plan_creation() {
    TEST_START("FFT Plan Creation");

    // Test valid sizes
    fft_plan_t *plan1 = fft_plan_create(8, FFT_FORWARD);
    fft_plan_t *plan2 = fft_plan_create(1024, FFT_INVERSE);

    if (plan1 && plan2) {
        // Check plan properties
        if (plan1->size == 8 && plan1->direction == FFT_FORWARD &&
            plan2->size == 1024 && plan2->direction == FFT_INVERSE) {
            TEST_PASS();
        } else {
            TEST_FAIL("Plan properties incorrect");
        }

        fft_plan_destroy(plan1);
        fft_plan_destroy(plan2);
    } else {
        TEST_FAIL("Plan creation failed");
    }

    // Test invalid sizes
    fft_plan_t *invalid1 = fft_plan_create(3, FFT_FORWARD);  // Not power of 2
    fft_plan_t *invalid2 = fft_plan_create(2000000, FFT_FORWARD);  // Too large

    if (!invalid1 && !invalid2) {
        TEST_PASS();
    } else {
        TEST_FAIL("Invalid size rejection failed");
    }

    TEST_END();
}

// Test FFT mathematical properties
void test_fft_mathematical_properties() {
    TEST_START("FFT Mathematical Properties");

    const int SIZE = 16;
    fft_complex_t input[SIZE];
    fft_complex_t output[SIZE];
    fft_complex_t reconstructed[SIZE];

    // Create test signal
    for (int i = 0; i < SIZE; i++) {
        double phase = 2.0 * M_PI * 2.0 * i / SIZE;  // 2 cycles
        input[i] = cos(phase) + sin(phase) * I;
    }

    // Forward FFT
    bool success1 = fft_forward(input, output, SIZE);
    if (!success1) {
        TEST_FAIL("Forward FFT failed");
        return;
    }

    // Inverse FFT
    bool success2 = fft_inverse(output, reconstructed, SIZE);
    if (!success2) {
        TEST_FAIL("Inverse FFT failed");
        return;
    }

    // Check reconstruction accuracy
    double max_error = 0.0;
    for (int i = 0; i < SIZE; i++) {
        fft_complex_t error = reconstructed[i] - input[i];
        double error_mag = fft_magnitude(error);
        if (error_mag > max_error) max_error = error_mag;
    }

    if (max_error < 1e-12) {
        TEST_PASS();
    } else {
        TEST_FAIL("Reconstruction accuracy too low");
        printf("    Max error: %.2e\n", max_error);
    }

    TEST_END();
}

// Test DC signal FFT
void test_dc_signal() {
    TEST_START("DC Signal FFT");

    const int SIZE = 8;
    fft_complex_t input[SIZE];
    fft_complex_t output[SIZE];

    // Create DC signal (constant value)
    for (int i = 0; i < SIZE; i++) {
        input[i] = 1.0 + 0.0 * I;
    }

    bool success = fft_forward(input, output, SIZE);
    if (!success) {
        TEST_FAIL("FFT failed");
        return;
    }

    // Check DC bin (should be SIZE)
    double dc_magnitude = fft_magnitude(output[0]);
    double dc_error = fabs(dc_magnitude - SIZE);

    // Check other bins (should be ~0)
    double max_other = 0.0;
    for (int i = 1; i < SIZE; i++) {
        double mag = fft_magnitude(output[i]);
        if (mag > max_other) max_other = mag;
    }

    if (dc_error < 1e-10 && max_other < 1e-10) {
        TEST_PASS();
    } else {
        TEST_FAIL("DC signal FFT incorrect");
        printf("    DC magnitude: %.6f (expected: %.1f)\n", dc_magnitude, (double)SIZE);
        printf("    Max other bin: %.2e\n", max_other);
    }

    TEST_END();
}

// Test Nyquist frequency
void test_nyquist_frequency() {
    TEST_START("Nyquist Frequency Test");

    const int SIZE = 8;
    fft_complex_t input[SIZE];
    fft_complex_t output[SIZE];

    // Create signal at Nyquist frequency (Fs/2)
    for (int i = 0; i < SIZE; i++) {
        double phase = M_PI * i;  // π * i (180° * i)
        input[i] = cos(phase) + 0.0 * I;  // Alternating: 1, -1, 1, -1, ...
    }

    bool success = fft_forward(input, output, SIZE);
    if (!success) {
        TEST_FAIL("FFT failed");
        return;
    }

    // Nyquist bin (SIZE/2) should contain all energy
    double nyquist_magnitude = fft_magnitude(output[SIZE/2]);

    // Other bins should be ~0
    double max_other = 0.0;
    for (int i = 0; i < SIZE; i++) {
        if (i != SIZE/2) {
            double mag = fft_magnitude(output[i]);
            if (mag > max_other) max_other = mag;
        }
    }

    if (fabs(nyquist_magnitude - SIZE) < 1e-10 && max_other < 1e-10) {
        TEST_PASS();
    } else {
        TEST_FAIL("Nyquist frequency test failed");
        printf("    Nyquist magnitude: %.6f (expected: %.1f)\n", nyquist_magnitude, (double)SIZE);
        printf("    Max other bin: %.2e\n", max_other);
    }

    TEST_END();
}

// Test frequency detection accuracy
void test_frequency_detection() {
    TEST_START("Frequency Detection Accuracy");

    const int SIZE = 1024;
    const double SAMPLE_RATE = 1000000.0;  // 1 MHz
    const double TARGET_FREQ = 100000.0;   // 100 kHz

    fft_complex_t input[SIZE];
    fft_complex_t output[SIZE];

    // Create pure tone
    for (int i = 0; i < SIZE; i++) {
        double t = (double)i / SAMPLE_RATE;
        double phase = 2.0 * M_PI * TARGET_FREQ * t;
        input[i] = cos(phase) + 0.0 * I;
    }

    bool success = fft_forward(input, output, SIZE);
    if (!success) {
        TEST_FAIL("FFT failed");
        return;
    }

    // Find peak
    double max_magnitude = 0;
    int peak_index = 0;
    for (int i = 0; i < SIZE/2; i++) {
        double mag = fft_magnitude(output[i]);
        if (mag > max_magnitude) {
            max_magnitude = mag;
            peak_index = i;
        }
    }

    // Calculate detected frequency
    double detected_freq = (double)peak_index * SAMPLE_RATE / SIZE;
    double freq_error = fabs(detected_freq - TARGET_FREQ);
    double relative_error = freq_error / TARGET_FREQ;

    // Theoretical bin
    int theoretical_bin = (int)round(TARGET_FREQ * SIZE / SAMPLE_RATE);
    int bin_error = abs(peak_index - theoretical_bin);

    if (bin_error <= 1 && relative_error < 0.01) {  // Within 1% error
        TEST_PASS();
        printf("    Detected: %.1f Hz (bin %d), Expected: %.1f Hz (bin %d)\n",
               detected_freq, peak_index, TARGET_FREQ, theoretical_bin);
    } else {
        TEST_FAIL("Frequency detection accuracy too low");
        printf("    Error: %.1f Hz (%.2f%%), Bin error: %d\n",
               freq_error, relative_error * 100, bin_error);
    }

    TEST_END();
}

// Test complex signal FFT
void test_complex_signal() {
    TEST_START("Complex Signal FFT");

    const int SIZE = 8;
    fft_complex_t input[SIZE];
    fft_complex_t output[SIZE];
    fft_complex_t reconstructed[SIZE];

    // Create complex signal
    for (int i = 0; i < SIZE; i++) {
        double phase = 2.0 * M_PI * i / SIZE;
        input[i] = cos(phase) + sin(phase) * I;
    }

    // Forward and inverse
    bool success1 = fft_forward(input, output, SIZE);
    bool success2 = fft_inverse(output, reconstructed, SIZE);

    if (!success1 || !success2) {
        TEST_FAIL("Complex FFT failed");
        return;
    }

    // Check reconstruction
    double max_error = 0.0;
    for (int i = 0; i < SIZE; i++) {
        fft_complex_t error = reconstructed[i] - input[i];
        double error_mag = fft_magnitude(error);
        if (error_mag > max_error) max_error = error_mag;
    }

    if (max_error < 1e-12) {
        TEST_PASS();
    } else {
        TEST_FAIL("Complex signal reconstruction failed");
        printf("    Max error: %.2e\n", max_error);
    }

    TEST_END();
}

// Test power spectrum calculation
void test_power_spectrum() {
    TEST_START("Power Spectrum Calculation");

    const int SIZE = 16;
    fft_complex_t fft_result[SIZE];
    double power_spectrum[SIZE];

    // Create simple FFT result
    for (int i = 0; i < SIZE; i++) {
        fft_result[i] = (double)i + 0.0 * I;  // Real values
    }

    // Calculate power spectrum
    bool success = fft_power_spectrum(fft_result, power_spectrum, SIZE, false);

    if (!success) {
        TEST_FAIL("Power spectrum calculation failed");
        return;
    }

    // Check that power spectrum is correct (magnitude squared)
    int all_correct = 1;
    for (int i = 0; i < SIZE; i++) {
        double expected_power = (double)(i * i);  // |i|^2
        double actual_power = power_spectrum[i];
        double error = fabs(actual_power - expected_power);

        if (error > 1e-10) {
            all_correct = 0;
            break;
        }
    }

    if (all_correct) {
        TEST_PASS();
    } else {
        TEST_FAIL("Power spectrum values incorrect");
    }

    TEST_END();
}

// Test I/Q conversion utilities
void test_iq_conversion() {
    TEST_START("I/Q Conversion Utilities");

    const int SIZE = 8;
    float iq_data[SIZE * 2];
    fft_complex_t complex_data[SIZE];

    // Create I/Q data
    for (int i = 0; i < SIZE; i++) {
        iq_data[i * 2] = (float)i;          // I component
        iq_data[i * 2 + 1] = (float)(i + 1); // Q component
    }

    // Convert to complex
    fft_iq_to_complex(iq_data, complex_data, SIZE, false);

    // Check conversion
    int conversion_correct = 1;
    for (int i = 0; i < SIZE; i++) {
        double expected_real = (double)i;
        double expected_imag = (double)(i + 1);
        double actual_real = creal(complex_data[i]);
        double actual_imag = cimag(complex_data[i]);

        if (fabs(actual_real - expected_real) > 1e-10 ||
            fabs(actual_imag - expected_imag) > 1e-10) {
            conversion_correct = 0;
            break;
        }
    }

    if (conversion_correct) {
        TEST_PASS();
    } else {
        TEST_FAIL("I/Q to complex conversion failed");
    }

    TEST_END();
}

// Main test runner
int main() {
    printf("=====================================\n");
    printf("IQ Lab - FFT Unit Tests\n");
    printf("=====================================\n\n");

    // Run all tests
    test_power_of_two();
    test_fft_plan_creation();
    test_fft_mathematical_properties();
    test_dc_signal();
    test_nyquist_frequency();
    test_frequency_detection();
    test_complex_signal();
    test_power_spectrum();
    test_iq_conversion();

    // Summary
    printf("=====================================\n");
    printf("Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("=====================================\n");

    if (tests_passed == tests_run) {
        printf("🎉 All FFT tests PASSED!\n");
        return EXIT_SUCCESS;
    } else {
        printf("❌ Some FFT tests FAILED!\n");
        return EXIT_FAILURE;
    }
}
