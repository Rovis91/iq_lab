/*
 * IQ Lab - Features Unit Tests
 *
 * Tests for the signal feature extraction module.
 * These tests validate the SNR estimation, bandwidth detection,
 * and modulation classification algorithms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "../../src/detect/features.h"

// Test data generation helpers
static double *generate_flat_spectrum(uint32_t size, double power) {
    double *spectrum = malloc(size * sizeof(double));
    if (!spectrum) return NULL;

    for (uint32_t i = 0; i < size; i++) {
        spectrum[i] = power;
    }
    return spectrum;
}

static double *generate_signal_spectrum(uint32_t size, uint32_t center_bin,
                                       uint32_t bandwidth_bins, double signal_power,
                                       double noise_power) {
    double *spectrum = generate_flat_spectrum(size, noise_power);
    if (!spectrum) return NULL;

    // Add a signal with triangular shape
    int32_t half_width = (int32_t)bandwidth_bins / 2;
    int32_t start_bin = (int32_t)center_bin - half_width;
    int32_t end_bin = (int32_t)center_bin + half_width;

    for (int32_t bin = start_bin; bin <= end_bin; bin++) {
        if (bin >= 0 && (uint32_t)bin < size) {
            double distance_from_center = fabs((double)bin - (double)center_bin) / half_width;
            double signal_level = signal_power * (1.0 - distance_from_center * 0.5);
            spectrum[bin] += signal_level;
        }
    }

    return spectrum;
}

// Test functions
static void test_features_initialization(void) {
    printf("Testing features initialization...\n");

    features_t features;
    bool result;

    // Test valid initialization
    result = features_init(&features, 4096, 2000000.0, 512);
    assert(result == true);
    assert(features.initialized == true);
    assert(features.fft_size == 4096);
    assert(features.sample_rate_hz == 2000000.0);
    assert(features.analysis_window_bins == 512);
    features_free(&features);

    // Test invalid parameters
    result = features_init(&features, 0, 2000000.0, 512); // Invalid FFT size
    assert(result == false);

    result = features_init(&features, 4096, 0.0, 512); // Invalid sample rate
    assert(result == false);

    result = features_init(&features, 4096, 2000000.0, 8192); // Window > FFT
    assert(result == false);

    printf("✓ Features initialization tests passed\n");
}

static void test_features_parameter_validation(void) {
    printf("Testing features parameter validation...\n");

    // Test valid parameters
    bool result = features_validate_params(4096, 2000000.0, 512);
    assert(result == true);

    // Test invalid parameters
    result = features_validate_params(0, 2000000.0, 512);
    assert(result == false);

    result = features_validate_params(4096, 0.0, 512);
    assert(result == false);

    result = features_validate_params(4096, 2000000.0, 8192); // Window > FFT
    assert(result == false);

    printf("✓ Features parameter validation tests passed\n");
}

static void test_features_snr_calculation(void) {
    printf("Testing features SNR calculation...\n");

    // Test normal SNR calculation
    double snr = features_calculate_snr(100.0, 1.0);
    assert(fabs(snr - 20.0) < 0.1); // Should be 20 dB

    // Test edge cases
    snr = features_calculate_snr(0.0, 1.0);
    assert(snr == -INFINITY);

    snr = features_calculate_snr(100.0, 0.0);
    assert(snr == -INFINITY);

    printf("✓ Features SNR calculation tests passed\n");
}

static void test_features_papr_calculation(void) {
    printf("Testing features PAPR calculation...\n");

    double spectrum[10] = {1.0, 2.0, 4.0, 8.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5};

    double papr = features_calculate_papr(spectrum, 0, 9);
    assert(papr > 0.0); // Should be positive

    // Test edge cases
    papr = features_calculate_papr(spectrum, 5, 5); // Single bin
    assert(fabs(papr - 0.0) < 0.1); // Should be 0 dB for constant power

    papr = features_calculate_papr(spectrum, 0, 0); // Invalid range
    assert(papr == 0.0);

    printf("✓ Features PAPR calculation tests passed\n");
}

static void test_features_spectral_flatness(void) {
    printf("Testing features spectral flatness...\n");

    double flat_spectrum[8] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    double tonal_spectrum[8] = {0.1, 0.1, 10.0, 0.1, 0.1, 0.1, 0.1, 0.1};

    double flatness_flat = features_calculate_spectral_flatness(flat_spectrum, 0, 7);
    double flatness_tonal = features_calculate_spectral_flatness(tonal_spectrum, 0, 7);

    assert(flatness_flat > flatness_tonal); // Flat spectrum should have higher flatness
    assert(flatness_flat <= 1.0 && flatness_flat >= 0.0);

    printf("✓ Features spectral flatness tests passed\n");
}

static void test_features_bandwidth_detection(void) {
    printf("Testing features bandwidth detection...\n");

    features_t features;
    bool result = features_init(&features, 1024, 2000000.0, 128);
    assert(result == true);

    // Create a spectrum with a known signal
    double *spectrum = generate_signal_spectrum(1024, 512, 50, 10.0, 1.0);
    assert(spectrum != NULL);

    uint32_t bandwidth_3db = features_detect_bandwidth(&features, spectrum, 512, "3db", 0.5);
    uint32_t bandwidth_occupied = features_detect_bandwidth(&features, spectrum, 512, "occupied", 0.99);

    assert(bandwidth_3db > 0);
    assert(bandwidth_occupied >= bandwidth_3db); // Occupied should be >= 3dB

    free(spectrum);
    features_free(&features);

    printf("✓ Features bandwidth detection tests passed\n");
}

static void test_features_modulation_classification(void) {
    printf("Testing features modulation classification...\n");

    features_result_t result;

    // Test FM classification (wide bandwidth)
    result.bandwidth_hz = 50000.0;
    result.valid = true;
    const char *mod = features_classify_modulation(&result);
    assert(strcmp(mod, "fm") == 0);

    // Test AM classification (medium bandwidth)
    result.bandwidth_hz = 10000.0;
    mod = features_classify_modulation(&result);
    assert(strcmp(mod, "am") == 0);

    // Test SSB classification (narrow bandwidth)
    result.bandwidth_hz = 3000.0;
    mod = features_classify_modulation(&result);
    assert(strcmp(mod, "ssb") == 0);

    // Test CW classification (very narrow bandwidth)
    result.bandwidth_hz = 500.0;
    mod = features_classify_modulation(&result);
    assert(strcmp(mod, "cw") == 0);

    // Test noise classification (very wide bandwidth)
    result.bandwidth_hz = 200000.0;
    mod = features_classify_modulation(&result);
    assert(strcmp(mod, "noise") == 0);

    // Test invalid result
    result.valid = false;
    mod = features_classify_modulation(&result);
    assert(strcmp(mod, "unknown") == 0);

    printf("✓ Features modulation classification tests passed\n");
}

static void test_features_spectrum_extraction(void) {
    printf("Testing features spectrum extraction...\n");

    features_t features;
    bool result = features_init(&features, 1024, 2000000.0, 128);
    assert(result == true);

    // Create a test spectrum with a signal
    double *spectrum = generate_signal_spectrum(1024, 512, 32, 100.0, 1.0);
    assert(spectrum != NULL);

    features_result_t extraction_result;
    result = features_extract_from_spectrum(&features, spectrum, 512, 64, &extraction_result);

    assert(result == true);
    assert(extraction_result.valid == true);
    assert(extraction_result.snr_db > 10.0); // Should have good SNR
    assert(extraction_result.bandwidth_hz > 0.0);
    assert(extraction_result.center_frequency_hz > 0.0);

    free(spectrum);
    features_free(&features);

    printf("✓ Features spectrum extraction tests passed\n");
}

static void test_features_config_string(void) {
    printf("Testing features configuration string...\n");

    features_t features;
    bool result = features_init(&features, 2048, 1000000.0, 256);
    assert(result == true);

    char buffer[256];
    result = features_get_config_string(&features, buffer, sizeof(buffer));
    assert(result == true);

    // Check that configuration string contains expected parameters
    assert(strstr(buffer, "FFT=2048") != NULL);
    assert(strstr(buffer, "SampleRate=1000000") != NULL);
    assert(strstr(buffer, "Window=256") != NULL);

    features_free(&features);

    printf("✓ Features configuration string test passed\n");
}

static void test_features_reset(void) {
    printf("Testing features reset functionality...\n");

    features_t features;
    bool result = features_init(&features, 1024, 2000000.0, 128);
    assert(result == true);
    assert(features.initialized == true);

    // Reset features
    features_reset(&features);
    assert(features.initialized == true); // Should still be initialized

    // Free features
    features_free(&features);
    assert(features.initialized == false);

    printf("✓ Features reset test passed\n");
}

// Main test runner
int main(int argc, char **argv) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning

    printf("Running Features Unit Tests\n");
    printf("===========================\n\n");

    test_features_initialization();
    test_features_parameter_validation();
    test_features_snr_calculation();
    test_features_papr_calculation();
    test_features_spectral_flatness();
    test_features_bandwidth_detection();
    test_features_modulation_classification();
    test_features_spectrum_extraction();
    test_features_config_string();
    test_features_reset();

    printf("\n===========================\n");
    printf("All features tests passed! ✓\n");
    printf("===========================\n");

    return 0;
}
