/*
 * IQ Lab - Window Functions Unit Tests
 *
 * Tests for window function generation and application
 * Covers coefficient accuracy, edge cases, and mathematical properties
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <assert.h>
#include "../../src/iq_core/window.h"

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

// Test window creation
void test_window_creation() {
    TEST_START("Window Creation");

    // Test all window types
    window_t *rect = window_create(WINDOW_RECTANGULAR, 1024);
    window_t *hann = window_create(WINDOW_HANN, 1024);
    window_t *hamming = window_create(WINDOW_HAMMING, 1024);
    window_t *blackman = window_create(WINDOW_BLACKMAN, 1024);
    window_t *bh = window_create(WINDOW_BLACKMAN_HARRIS, 1024);
    window_t *flat = window_create(WINDOW_FLAT_TOP, 1024);

    if (rect && hann && hamming && blackman && bh && flat) {
        // Check sizes
        if (rect->size == 1024 && hann->size == 1024 && hamming->size == 1024 &&
            blackman->size == 1024 && bh->size == 1024 && flat->size == 1024) {
            TEST_PASS();
        } else {
            TEST_FAIL("Window sizes incorrect");
        }

        window_destroy(rect);
        window_destroy(hann);
        window_destroy(hamming);
        window_destroy(blackman);
        window_destroy(bh);
        window_destroy(flat);
    } else {
        TEST_FAIL("Window creation failed");
        if (rect) window_destroy(rect);
        if (hann) window_destroy(hann);
        if (hamming) window_destroy(hamming);
        if (blackman) window_destroy(blackman);
        if (bh) window_destroy(bh);
        if (flat) window_destroy(flat);
    }

    TEST_END();
}

// Test window size 1 edge case
void test_window_size_1() {
    TEST_START("Window Size 1 Edge Case");

    // Test all window types with size 1
    window_t *rect = window_create(WINDOW_RECTANGULAR, 1);
    window_t *hann = window_create(WINDOW_HANN, 1);
    window_t *hamming = window_create(WINDOW_HAMMING, 1);
    window_t *blackman = window_create(WINDOW_BLACKMAN, 1);
    window_t *bh = window_create(WINDOW_BLACKMAN_HARRIS, 1);
    window_t *flat = window_create(WINDOW_FLAT_TOP, 1);

    if (rect && hann && hamming && blackman && bh && flat) {
        // All size 1 windows should have coefficient = 1.0
        int all_correct = 1;
        double expected = 1.0;

        if (fabs(window_coefficient(rect, 0) - expected) > 1e-10) all_correct = 0;
        if (fabs(window_coefficient(hann, 0) - expected) > 1e-10) all_correct = 0;
        if (fabs(window_coefficient(hamming, 0) - expected) > 1e-10) all_correct = 0;
        if (fabs(window_coefficient(blackman, 0) - expected) > 1e-10) all_correct = 0;
        if (fabs(window_coefficient(bh, 0) - expected) > 1e-10) all_correct = 0;
        if (fabs(window_coefficient(flat, 0) - expected) > 1e-10) all_correct = 0;

        if (all_correct) {
            TEST_PASS();
        } else {
            TEST_FAIL("Size 1 window coefficients incorrect");
            printf("    Rectangular: %.6f\n", window_coefficient(rect, 0));
            printf("    Hann: %.6f\n", window_coefficient(hann, 0));
            printf("    Hamming: %.6f\n", window_coefficient(hamming, 0));
            printf("    Blackman: %.6f\n", window_coefficient(blackman, 0));
            printf("    Blackman-Harris: %.6f\n", window_coefficient(bh, 0));
            printf("    Flat-top: %.6f\n", window_coefficient(flat, 0));
        }

        window_destroy(rect);
        window_destroy(hann);
        window_destroy(hamming);
        window_destroy(blackman);
        window_destroy(bh);
        window_destroy(flat);
    } else {
        TEST_FAIL("Size 1 window creation failed");
    }

    TEST_END();
}

// Test rectangular window properties
void test_rectangular_window() {
    TEST_START("Rectangular Window Properties");

    const int SIZE = 1024;
    window_t *rect = window_create(WINDOW_RECTANGULAR, SIZE);

    if (rect) {
        // All coefficients should be 1.0
        int all_ones = 1;
        for (int i = 0; i < SIZE; i++) {
            if (fabs(window_coefficient(rect, i) - 1.0) > 1e-10) {
                all_ones = 0;
                break;
            }
        }

        // Coherent gain should be 1.0
        double coherent_gain = window_coherent_gain(rect);
        double cg_error = fabs(coherent_gain - 1.0);

        // ENBW should be 1.0
        double enbw = window_enbw_bins(rect);
        double enbw_error = fabs(enbw - 1.0);

        if (all_ones && cg_error < 1e-10 && enbw_error < 1e-10) {
            TEST_PASS();
            printf("    Coherent gain: %.6f\n", coherent_gain);
            printf("    ENBW: %.3f bins\n", enbw);
        } else {
            TEST_FAIL("Rectangular window properties incorrect");
            printf("    All coefficients 1.0: %s\n", all_ones ? "YES" : "NO");
            printf("    Coherent gain: %.6f (expected: 1.0)\n", coherent_gain);
            printf("    ENBW: %.3f (expected: 1.0)\n", enbw);
        }

        window_destroy(rect);
    } else {
        TEST_FAIL("Rectangular window creation failed");
    }

    TEST_END();
}

// Test Hann window symmetry and properties
void test_hann_window() {
    TEST_START("Hann Window Properties");

    const int SIZE = 1024;
    window_t *hann = window_create(WINDOW_HANN, SIZE);

    if (hann) {
        // Check symmetry
        int symmetric = 1;
        for (int i = 0; i < SIZE/2; i++) {
            double left = window_coefficient(hann, i);
            double right = window_coefficient(hann, SIZE - 1 - i);
            if (fabs(left - right) > 1e-10) {
                symmetric = 0;
                break;
            }
        }

        // Check endpoints (should be ~0)
        double start_coeff = window_coefficient(hann, 0);
        double end_coeff = window_coefficient(hann, SIZE - 1);
        int endpoints_zero = (fabs(start_coeff) < 1e-10 && fabs(end_coeff) < 1e-10);

        // Check middle value (should be 1.0)
        double middle_coeff = window_coefficient(hann, SIZE/2);
        int middle_one = fabs(middle_coeff - 1.0) < 1e-10;

        // Check coherent gain (~0.5)
        double coherent_gain = window_coherent_gain(hann);
        int cg_correct = fabs(coherent_gain - 0.5) < 0.01;

        if (symmetric && endpoints_zero && middle_one && cg_correct) {
            TEST_PASS();
            printf("    Symmetric: YES\n");
            printf("    Endpoints ~0: YES\n");
            printf("    Middle = 1.0: YES\n");
            printf("    Coherent gain: %.6f\n", coherent_gain);
        } else {
            TEST_FAIL("Hann window properties incorrect");
            printf("    Symmetric: %s\n", symmetric ? "YES" : "NO");
            printf("    Endpoints ~0: %s\n", endpoints_zero ? "YES" : "NO");
            printf("    Middle = 1.0: %s\n", middle_one ? "YES" : "NO");
            printf("    Coherent gain: %.6f (expected: ~0.5)\n", coherent_gain);
        }

        window_destroy(hann);
    } else {
        TEST_FAIL("Hann window creation failed");
    }

    TEST_END();
}

// Test window application to real signal
void test_window_application_real() {
    TEST_START("Window Application (Real Signal)");

    const int SIZE = 8;
    window_t *hann = window_create(WINDOW_HANN, SIZE);

    if (hann) {
        // Create test signal
        double input[SIZE] = {1, 2, 3, 4, 5, 6, 7, 8};
        double output[SIZE];

        // Apply window
        bool success = window_apply_real(hann, input, output);

        if (success) {
            // Check that window was applied (output != input)
            int window_applied = 0;
            for (int i = 0; i < SIZE; i++) {
                if (fabs(output[i] - input[i]) > 1e-10) {
                    window_applied = 1;
                    break;
                }
            }

            // Check endpoints are attenuated
            double start_attenuation = output[0] / input[0];
            double end_attenuation = output[SIZE-1] / input[SIZE-1];
            int endpoints_attenuated = (start_attenuation < 0.1 && end_attenuation < 0.1);

            if (window_applied && endpoints_attenuated) {
                TEST_PASS();
                printf("    Window applied: YES\n");
                printf("    Endpoints attenuated: YES\n");
                printf("    Start attenuation: %.3f\n", start_attenuation);
                printf("    End attenuation: %.3f\n", end_attenuation);
            } else {
                TEST_FAIL("Window application incorrect");
                printf("    Window applied: %s\n", window_applied ? "YES" : "NO");
                printf("    Endpoints attenuated: %s\n", endpoints_attenuated ? "YES" : "NO");
            }
        } else {
            TEST_FAIL("Window application failed");
        }

        window_destroy(hann);
    } else {
        TEST_FAIL("Window creation failed");
    }

    TEST_END();
}

// Test window application to complex signal
void test_window_application_complex() {
    TEST_START("Window Application (Complex Signal)");

    const int SIZE = 4;
    window_t *hamming = window_create(WINDOW_HAMMING, SIZE);

    if (hamming) {
        // Create complex test signal
        fft_complex_t input[SIZE] = {
            1.0 + 1.0*I,
            2.0 + 2.0*I,
            3.0 + 3.0*I,
            4.0 + 4.0*I
        };
        fft_complex_t output[SIZE];

        // Apply window
        bool success = window_apply_complex(hamming, input, output);

        if (success) {
            // Check that both real and imaginary parts are scaled
            int scaling_correct = 1;
            for (int i = 0; i < SIZE; i++) {
                double input_mag = fft_magnitude(input[i]);
                double output_mag = fft_magnitude(output[i]);
                double scale_factor = output_mag / input_mag;

                // Scale factor should match window coefficient
                double expected_scale = window_coefficient(hamming, i);

                if (fabs(scale_factor - expected_scale) > 1e-10) {
                    scaling_correct = 0;
                    break;
                }
            }

            if (scaling_correct) {
                TEST_PASS();
                printf("    Complex scaling correct: YES\n");
                for (int i = 0; i < SIZE; i++) {
                    double scale = window_coefficient(hamming, i);
                    printf("    [%d]: scale=%.6f\n", i, scale);
                }
            } else {
                TEST_FAIL("Complex scaling incorrect");
            }
        } else {
            TEST_FAIL("Complex window application failed");
        }

        window_destroy(hamming);
    } else {
        TEST_FAIL("Window creation failed");
    }

    TEST_END();
}

// Test window creation from name
void test_window_name_creation() {
    TEST_START("Window Creation from Name");

    // Test valid names
    const char *valid_names[] = {"hann", "HANN", "hamming", "blackman", "blackman-harris", "flat-top"};
    const window_type_t expected_types[] = {
        WINDOW_HANN, WINDOW_HANN, WINDOW_HAMMING,
        WINDOW_BLACKMAN, WINDOW_BLACKMAN_HARRIS, WINDOW_FLAT_TOP
    };

    int name_tests_correct = 1;
    for (int i = 0; i < sizeof(valid_names)/sizeof(valid_names[0]); i++) {
        window_t *win = window_create_from_name(valid_names[i], 256);
        if (win) {
            if (win->type != expected_types[i]) {
                name_tests_correct = 0;
            }
            window_destroy(win);
        } else {
            name_tests_correct = 0;
        }
    }

    // Test invalid name (should default to Hann)
    window_t *invalid_win = window_create_from_name("invalid_window", 256);
    int invalid_handled = (invalid_win && invalid_win->type == WINDOW_HANN);

    if (invalid_win) window_destroy(invalid_win);

    if (name_tests_correct && invalid_handled) {
        TEST_PASS();
        printf("    Valid names: handled correctly\n");
        printf("    Invalid names: default to Hann\n");
    } else {
        TEST_FAIL("Window name creation failed");
        printf("    Valid names correct: %s\n", name_tests_correct ? "YES" : "NO");
        printf("    Invalid name handling: %s\n", invalid_handled ? "YES" : "NO");
    }

    TEST_END();
}

// Test window property calculations
void test_window_properties() {
    TEST_START("Window Property Calculations");

    window_t *windows[] = {
        window_create(WINDOW_RECTANGULAR, 1024),
        window_create(WINDOW_HANN, 1024),
        window_create(WINDOW_BLACKMAN, 1024)
    };

    const char *names[] = {"Rectangular", "Hann", "Blackman"};

    int all_properties_correct = 1;

    for (int i = 0; i < 3; i++) {
        if (windows[i]) {
            double cg = window_coherent_gain(windows[i]);
            double enbw = window_enbw_bins(windows[i]);
            double scallop = window_scalloping_loss_db(windows[i]);

            printf("    %s:\n", names[i]);
            printf("      Coherent gain: %.6f\n", cg);
            printf("      ENBW: %.3f bins\n", enbw);
            printf("      Scalloping loss: %.3f dB\n", scallop);

            // Basic sanity checks
            if (cg <= 0 || cg > 1.1 || enbw <= 0 || isnan(scallop)) {
                all_properties_correct = 0;
            }

            window_destroy(windows[i]);
        } else {
            all_properties_correct = 0;
        }
    }

    if (all_properties_correct) {
        TEST_PASS();
    } else {
        TEST_FAIL("Window property calculations failed");
    }

    TEST_END();
}

// Test error handling
void test_window_error_handling() {
    TEST_START("Window Error Handling");

    // Test invalid size
    window_t *invalid_size = window_create(WINDOW_HANN, 0);
    if (invalid_size) {
        TEST_FAIL("Should reject size 0");
        window_destroy(invalid_size);
        return;
    }

    // Test NULL pointers in application
    window_t *win = window_create(WINDOW_HANN, 8);
    if (!win) {
        TEST_FAIL("Window creation failed");
        return;
    }

    double dummy_data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    double output_data[8];

    // These should all fail gracefully
    bool result1 = window_apply_real(NULL, dummy_data, output_data);
    bool result2 = window_apply_real(win, NULL, output_data);
    bool result3 = window_apply_real(win, dummy_data, NULL);

    if (!result1 && !result2 && !result3) {
        TEST_PASS();
        printf("    NULL pointer handling: correct\n");
    } else {
        TEST_FAIL("NULL pointer handling failed");
    }

    // Test out-of-bounds coefficient access
    double coeff1 = window_coefficient(win, -1);  // Should return 0
    double coeff2 = window_coefficient(win, 8);   // Should return 0

    if (fabs(coeff1) < 1e-10 && fabs(coeff2) < 1e-10) {
        TEST_PASS();
        printf("    Out-of-bounds access: handled correctly\n");
    } else {
        TEST_FAIL("Out-of-bounds access not handled");
    }

    window_destroy(win);
    TEST_END();
}

// Main test runner
int main() {
    printf("=====================================\n");
    printf("IQ Lab - Window Functions Unit Tests\n");
    printf("=====================================\n\n");

    // Run all tests
    test_window_creation();
    test_window_size_1();
    test_rectangular_window();
    test_hann_window();
    test_window_application_real();
    test_window_application_complex();
    test_window_name_creation();
    test_window_properties();
    test_window_error_handling();

    // Summary
    printf("=====================================\n");
    printf("Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("=====================================\n");

    if (tests_passed == tests_run) {
        printf("🎉 All Window tests PASSED!\n");
        return EXIT_SUCCESS;
    } else {
        printf("❌ Some Window tests FAILED!\n");
        return EXIT_FAILURE;
    }
}
