/*
 * IQ Lab - Signal Feature Extraction Header
 *
 * Purpose: Extract signal features for SNR estimation and modulation classification
 * Author: IQ Lab Development Team
 *
 * This module provides comprehensive signal feature extraction capabilities
 * essential for signal analysis, SNR estimation, and modulation classification.
 * It analyzes detected signals to extract meaningful characteristics that
 * help characterize signal quality and type.
 *
 * Key Features:
 * - SNR estimation using signal and noise power analysis
 * - Bandwidth calculation with various methods (-3dB, occupied bandwidth)
 * - Peak-to-average power ratio for signal characterization
 * - Spectral flatness measure for noise-like vs tonal signals
 * - Modulation type hints based on spectral properties
 * - Real-time feature extraction from FFT spectra
 *
 * Usage:
 *   features_t features;
 *   features_init(&features, fft_size, sample_rate);
 *   features_extract_from_spectrum(&features, power_spectrum, center_bin, bandwidth_bins);
 *   double snr = features_get_snr(&features);
 *
 * Technical Details:
 * - SNR Calculation: Signal power vs local noise floor estimation
 * - Bandwidth Methods: -3dB points, 99% power containment, spectral edges
 * - PAPR Analysis: Peak-to-average ratio for modulation characterization
 * - Spectral Flatness: Wiener entropy for noise-like signal detection
 *
 * Performance:
 * - O(B) complexity where B is bandwidth in bins
 * - Memory efficient with fixed-size analysis buffers
 * - Suitable for real-time processing of detected signals
 *
 * Applications:
 * - Signal quality assessment and SNR reporting
 * - Automatic modulation classification
 * - Interference detection and characterization
 * - Signal prioritization and filtering
 *
 * Dependencies: stdlib.h, stdbool.h, stdint.h, math.h
 * Thread Safety: Not thread-safe (single feature extractor per thread)
 * Performance: O(B) per feature extraction, suitable for real-time SDR
 */

#ifndef IQ_LAB_FEATURES_H
#define IQ_LAB_FEATURES_H

#include <stdint.h>
#include <stdbool.h>

// Feature extraction result structure
typedef struct {
    // Signal quality metrics
    double snr_db;                    // Signal-to-noise ratio in dB
    double peak_power_dbfs;          // Peak power in dBFS
    double avg_power_dbfs;           // Average power in dBFS
    double noise_floor_dbfs;         // Local noise floor estimate

    // Bandwidth metrics
    double bandwidth_hz;             // Signal bandwidth in Hz
    double bandwidth_3db_hz;         // -3dB bandwidth in Hz
    double bandwidth_occupied_hz;    // 99% occupied bandwidth in Hz

    // Spectral characteristics
    double peak_to_avg_ratio;        // Peak-to-average power ratio
    double spectral_flatness;        // Spectral flatness measure (0-1)
    double spectral_centroid;        // Spectral centroid (normalized)
    double spectral_spread;          // Spectral spread (normalized)

    // Modulation hints
    double modulation_confidence;    // Confidence in modulation classification (0-1)
    const char *modulation_hint;     // Suggested modulation type
    double fm_deviation_hz;          // Estimated FM deviation (if applicable)

    // Signal statistics
    uint32_t signal_bins;            // Number of bins containing signal
    uint32_t total_bins;             // Total bins in analysis window
    double center_frequency_hz;      // Center frequency of signal

    // Analysis metadata
    bool valid;                      // Whether analysis was successful
    double analysis_time_s;          // Time spent on analysis
} features_result_t;

// Feature extraction configuration
typedef struct {
    // Analysis parameters
    uint32_t fft_size;               // Size of FFT frame
    double sample_rate_hz;           // Sample rate in Hz
    uint32_t analysis_window_bins;   // Size of analysis window in bins

    // SNR estimation parameters
    double noise_estimation_factor;  // Factor for noise floor estimation (0.1-0.5)
    uint32_t noise_bins_margin;      // Bins to exclude around signal for noise est

    // Bandwidth detection parameters
    double bandwidth_3db_threshold;  // -3dB threshold relative to peak (0.5)
    double bandwidth_occupied_threshold; // Occupied bandwidth threshold (0.99)

    // Internal state
    bool initialized;                // Whether extractor is properly initialized
} features_t;

// Public API functions

/*
 * Initialize feature extraction with specified parameters
 *
 * Parameters:
 *   features         - Pointer to feature extractor to initialize
 *   fft_size         - Size of FFT frame
 *   sample_rate_hz   - Sample rate in Hz
 *   analysis_window  - Size of analysis window in bins (0 for auto)
 *
 * Returns:
 *   true on success, false on parameter error
 */
bool features_init(features_t *features, uint32_t fft_size, double sample_rate_hz,
                   uint32_t analysis_window);

/*
 * Extract features from a power spectrum around a detected signal
 *
 * Parameters:
 *   features       - Pointer to initialized feature extractor
 *   power_spectrum - FFT power spectrum in linear scale
 *   center_bin     - Center bin index of the detected signal
 *   bandwidth_bins - Estimated bandwidth in bins (0 for auto-detection)
 *   result         - Pointer to store extraction results
 *
 * Returns:
 *   true on success, false on error
 */
bool features_extract_from_spectrum(features_t *features, const double *power_spectrum,
                                   uint32_t center_bin, uint32_t bandwidth_bins,
                                   features_result_t *result);

/*
 * Extract features from a signal segment in time domain
 *
 * Parameters:
 *   features       - Pointer to initialized feature extractor
 *   iq_samples     - Complex IQ samples (interleaved I/Q)
 *   num_samples    - Number of IQ sample pairs
 *   sample_rate_hz - Sample rate of the IQ data
 *   result         - Pointer to store extraction results
 *
 * Returns:
 *   true on success, false on error
 */
bool features_extract_from_iq(features_t *features, const float *iq_samples,
                              uint32_t num_samples, double sample_rate_hz,
                              features_result_t *result);

/*
 * Calculate SNR from signal and noise power measurements
 *
 * Parameters:
 *   signal_power - Signal power in linear scale
 *   noise_power  - Noise power in linear scale
 *
 * Returns:
 *   SNR in dB, or -INFINITY if noise_power <= 0
 */
double features_calculate_snr(double signal_power, double noise_power);

/*
 * Estimate local noise floor around a signal
 *
 * Parameters:
 *   features       - Pointer to feature extractor
 *   power_spectrum - FFT power spectrum in linear scale
 *   center_bin     - Center bin of signal
 *   bandwidth_bins - Signal bandwidth in bins
 *
 * Returns:
 *   Estimated noise floor in linear scale
 */
double features_estimate_noise_floor(const features_t *features, const double *power_spectrum,
                                    uint32_t center_bin, uint32_t bandwidth_bins);

/*
 * Detect signal bandwidth using various methods
 *
 * Parameters:
 *   features       - Pointer to feature extractor
 *   power_spectrum - FFT power spectrum in linear scale
 *   center_bin     - Center bin of signal
 *   method         - Bandwidth detection method ("3db", "occupied", "edge")
 *   threshold      - Detection threshold (method-dependent)
 *
 * Returns:
 *   Bandwidth in bins, or 0 on error
 */
uint32_t features_detect_bandwidth(const features_t *features, const double *power_spectrum,
                                  uint32_t center_bin, const char *method, double threshold);

/*
 * Classify modulation type based on extracted features
 *
 * Parameters:
 *   result - Pointer to feature extraction results
 *
 * Returns:
 *   Modulation type string ("fm", "am", "ssb", "cw", "noise", "unknown")
 */
const char *features_classify_modulation(const features_result_t *result);

/*
 * Get peak-to-average power ratio from spectrum
 *
 * Parameters:
 *   power_spectrum - FFT power spectrum in linear scale
 *   start_bin      - Starting bin index
 *   end_bin        - Ending bin index
 *
 * Returns:
 *   PAPR in dB, or 0.0 on error
 */
double features_calculate_papr(const double *power_spectrum, uint32_t start_bin, uint32_t end_bin);

/*
 * Calculate spectral flatness measure (Wiener entropy)
 *
 * Parameters:
 *   power_spectrum - FFT power spectrum in linear scale
 *   start_bin      - Starting bin index
 *   end_bin        - Ending bin index
 *
 * Returns:
 *   Spectral flatness (0-1), where 1.0 is perfectly flat (white noise)
 */
double features_calculate_spectral_flatness(const double *power_spectrum,
                                           uint32_t start_bin, uint32_t end_bin);

/*
 * Reset feature extractor state
 *
 * Parameters:
 *   features - Pointer to feature extractor to reset
 */
void features_reset(features_t *features);

/*
 * Free feature extractor resources
 *
 * Parameters:
 *   features - Pointer to feature extractor to free
 */
void features_free(features_t *features);

/*
 * Get feature extractor configuration as string
 *
 * Parameters:
 *   features    - Pointer to feature extractor
 *   buffer      - Output buffer for configuration string
 *   buffer_size - Size of output buffer
 *
 * Returns:
 *   true on success, false if buffer too small
 */
bool features_get_config_string(const features_t *features, char *buffer, uint32_t buffer_size);

/*
 * Validate feature extraction parameters
 *
 * Parameters:
 *   fft_size         - Size of FFT frame
 *   sample_rate_hz   - Sample rate in Hz
 *   analysis_window  - Size of analysis window in bins
 *
 * Returns:
 *   true if parameters are valid, false otherwise
 */
bool features_validate_params(uint32_t fft_size, double sample_rate_hz, uint32_t analysis_window);

#endif /* IQ_LAB_FEATURES_H */
