/*
 * IQ Lab - CFAR OS Unit Tests
 *
 * Tests for the OS-CFAR (Ordered Statistics Constant False Alarm Rate) detector.
 * These tests validate the core detection algorithm, parameter handling, and
 * edge cases to ensure robust signal detection performance.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "../../src/detect/cfar_os.h"

// Test data generation helpers
static double *generate_noise_spectrum(uint32_t size, double noise_power) {
    double *spectrum = malloc(size * sizeof(double));
    if (!spectrum) return NULL;

    for (uint32_t i = 0; i < size; i++) {
        spectrum[i] = noise_power;
    }
    return spectrum;
}

static double *generate_signal_spectrum(uint32_t size, uint32_t signal_bin,
                                       double signal_power, double noise_power) {
    double *spectrum = generate_noise_spectrum(size, noise_power);
    if (!spectrum) return NULL;

    spectrum[signal_bin] = signal_power;
    return spectrum;
}

// Test functions
static void test_cfar_os_initialization(void) {
    printf("Testing CFAR OS initialization...\n");

    cfar_os_t cfar;
    bool result;

    // Test valid initialization
    result = cfar_os_init(&cfar, 4096, 1e-3, 16, 2, 8);
    assert(result == true);
    assert(cfar.initialized == true);
    assert(cfar.fft_size == 4096);
    assert(cfar.pfa == 1e-3);
    assert(cfar.ref_cells == 16);
    assert(cfar.guard_cells == 2);
    assert(cfar.os_rank == 8);
    cfar_os_free(&cfar);

    // Test invalid parameters
    result = cfar_os_init(&cfar, 0, 1e-3, 16, 2, 8); // Invalid FFT size
    assert(result == false);

    result = cfar_os_init(&cfar, 4096, 0.0, 16, 2, 8); // Invalid PFA
    assert(result == false);

    result = cfar_os_init(&cfar, 4096, 1e-3, 0, 2, 8); // Invalid ref cells
    assert(result == false);

    result = cfar_os_init(&cfar, 4096, 1e-3, 16, 20, 8); // Guard > ref cells
    assert(result == false);

    result = cfar_os_init(&cfar, 4096, 1e-3, 16, 2, 0); // Invalid OS rank
    assert(result == false);

    printf("✓ CFAR OS initialization tests passed\n");
}

static void test_cfar_os_parameter_validation(void) {
    printf("Testing CFAR OS parameter validation...\n");

    // Test valid parameters
    bool result = cfar_os_validate_params(4096, 1e-3, 16, 2, 8);
    assert(result == true);

    // Test invalid parameters
    result = cfar_os_validate_params(0, 1e-3, 16, 2, 8);
    assert(result == false);

    result = cfar_os_validate_params(4096, 0.0, 16, 2, 8);
    assert(result == false);

    result = cfar_os_validate_params(4096, 1e-3, 0, 2, 8);
    assert(result == false);

    result = cfar_os_validate_params(4096, 1e-3, 16, 20, 8); // Guard > ref
    assert(result == false);

    result = cfar_os_validate_params(4096, 1e-3, 16, 2, 0);
    assert(result == false);

    result = cfar_os_validate_params(4096, 1e-3, 16, 2, 20); // OS rank > ref
    assert(result == false);

    printf("✓ CFAR OS parameter validation tests passed\n");
}

static void test_cfar_os_noise_only_detection(void) {
    printf("Testing CFAR OS noise-only detection...\n");

    cfar_os_t cfar;
    bool result = cfar_os_init(&cfar, 1024, 1e-3, 8, 1, 4);
    assert(result == true);

    // Generate pure noise spectrum
    double *spectrum = generate_noise_spectrum(1024, 1.0);
    assert(spectrum != NULL);

    // Process frame - should detect no signals
    cfar_detection_t detections[10];
    uint32_t num_detections = cfar_os_process_frame(&cfar, spectrum, detections, 10);

    // With pure noise, we expect very few false detections at PFA=1e-3
    // Allow up to 1-2 false detections for statistical variation
    assert(num_detections <= 3);

    free(spectrum);
    cfar_os_free(&cfar);

    printf("✓ CFAR OS noise-only detection test passed (%u false detections)\n", num_detections);
}

static void test_cfar_os_signal_detection(void) {
    printf("Testing CFAR OS signal detection...\n");

    cfar_os_t cfar;
    bool result = cfar_os_init(&cfar, 1024, 1e-3, 8, 1, 4);
    assert(result == true);

    // Generate spectrum with strong signal
    double *spectrum = generate_signal_spectrum(1024, 512, 100.0, 1.0);
    assert(spectrum != NULL);

    // Process frame - should detect the signal
    cfar_detection_t detections[10];
    uint32_t num_detections = cfar_os_process_frame(&cfar, spectrum, detections, 10);

    // Should detect at least one signal
    assert(num_detections >= 1);

    // The detection should be near bin 512
    bool found_signal = false;
    for (uint32_t i = 0; i < num_detections; i++) {
        if (abs((int)detections[i].bin_index - 512) <= 5) { // Within 5 bins
            found_signal = true;
            // Verify SNR estimate
            assert(detections[i].snr_estimate > 10.0); // Should have good SNR
            assert(detections[i].confidence > 0.5); // Should have high confidence
            break;
        }
    }
    assert(found_signal == true);

    free(spectrum);
    cfar_os_free(&cfar);

    printf("✓ CFAR OS signal detection test passed (%u detections)\n", num_detections);
}

static void test_cfar_os_threshold_calculation(void) {
    printf("Testing CFAR OS threshold calculation...\n");

    cfar_os_t cfar;
    bool result = cfar_os_init(&cfar, 1024, 1e-3, 8, 1, 4);
    assert(result == true);

    // Generate noise spectrum
    double *spectrum = generate_noise_spectrum(1024, 1.0);
    assert(spectrum != NULL);

    // Test threshold calculation for various bins
    for (uint32_t bin = 10; bin < 1014; bin += 100) {
        double threshold = cfar_os_get_threshold(&cfar, spectrum, bin);
        assert(threshold > 0.0);
        assert(threshold < 10.0); // Should be reasonable for noise power = 1.0
    }

    // Test edge cases
    double threshold = cfar_os_get_threshold(&cfar, spectrum, 0); // First bin
    assert(threshold > 0.0);

    threshold = cfar_os_get_threshold(&cfar, spectrum, 1023); // Last bin
    assert(threshold > 0.0);

    free(spectrum);
    cfar_os_free(&cfar);

    printf("✓ CFAR OS threshold calculation tests passed\n");
}

static void test_cfar_os_config_string(void) {
    printf("Testing CFAR OS configuration string...\n");

    cfar_os_t cfar;
    bool result = cfar_os_init(&cfar, 2048, 1e-4, 12, 2, 6);
    assert(result == true);

    char buffer[256];
    result = cfar_os_get_config_string(&cfar, buffer, sizeof(buffer));
    assert(result == true);

    // Check that configuration string contains expected parameters
    assert(strstr(buffer, "FFT=2048") != NULL);
    assert(strstr(buffer, "PFA=1.00e-04") != NULL);
    assert(strstr(buffer, "RefCells=12") != NULL);
    assert(strstr(buffer, "GuardCells=2") != NULL);
    assert(strstr(buffer, "OS-Rank=6") != NULL);

    cfar_os_free(&cfar);

    printf("✓ CFAR OS configuration string test passed\n");
}

static void test_cfar_os_reset(void) {
    printf("Testing CFAR OS reset functionality...\n");

    cfar_os_t cfar;
    bool result = cfar_os_init(&cfar, 1024, 1e-3, 8, 1, 4);
    assert(result == true);

    // Generate and process a spectrum to modify internal state
    double *spectrum = generate_signal_spectrum(1024, 256, 10.0, 1.0);
    cfar_detection_t detections[5];
    uint32_t num_detections = cfar_os_process_frame(&cfar, spectrum, detections, 5);

    // Reset detector
    cfar_os_reset(&cfar);

    // Verify detector is still functional after reset
    num_detections = cfar_os_process_frame(&cfar, spectrum, detections, 5);
    assert(num_detections >= 1); // Should still detect signal

    free(spectrum);
    cfar_os_free(&cfar);

    printf("✓ CFAR OS reset test passed\n");
}

// Main test runner
int main(int argc, char **argv) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    printf("Running CFAR OS Unit Tests\n");
    printf("==========================\n\n");

    test_cfar_os_initialization();
    test_cfar_os_parameter_validation();
    test_cfar_os_noise_only_detection();
    test_cfar_os_signal_detection();
    test_cfar_os_threshold_calculation();
    test_cfar_os_config_string();
    test_cfar_os_reset();

    printf("\n==========================\n");
    printf("All CFAR OS tests passed! ✓\n");
    printf("==========================\n");

    return 0;
}
