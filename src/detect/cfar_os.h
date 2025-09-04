/*
 * IQ Lab - OS-CFAR Detector Header
 *
 * Purpose: Ordered Statistics Constant False Alarm Rate signal detection
  *
 *
 * This module implements the OS-CFAR (Ordered Statistics Constant False Alarm Rate)
 * algorithm for detecting signals in frequency domain power spectra. OS-CFAR is
 * particularly effective for detecting signals in non-homogeneous noise environments
 * where traditional CFAR methods struggle.
 *
 * Key Features:
 * - Sliding window OS-CFAR detection with configurable parameters
 * - Ordered statistics method for robust threshold calculation
 * - Configurable Probability of False Alarm (PFA) control
 * - Training cell and guard cell management
 * - Real-time detection with low latency
 *
 * Usage:
 *   cfar_os_t cfar;
 *   cfar_os_init(&cfar, fft_size, pfa, ref_cells, guard_cells, os_rank);
 *   cfar_os_process_frame(&cfar, power_spectrum, detections);
 *
 * Algorithm Overview:
 * 1. For each Cell Under Test (CUT) in the spectrum:
 *    - Select training cells around the CUT (excluding guard cells)
 *    - Sort training cell values in ascending order
 *    - Select the k-th highest value (OS-CFAR rank parameter)
 *    - Scale by CFAR constant to achieve desired PFA
 *    - Compare CUT power against adaptive threshold
 *
 * Technical Details:
 * - PFA Control: CFAR constant derived from ordered statistics theory
 * - Training Window: ref_cells cells on each side of CUT
 * - Guard Cells: guard_cells cells immediately adjacent to CUT (excluded)
 * - OS Rank: k parameter selects robustness vs sensitivity tradeoff
 *
 * Performance:
 * - O(N log N) complexity per frame (dominated by sorting)
 * - Memory efficient with fixed-size state
 * - Suitable for real-time processing up to high FFT sizes
 *
 * Applications:
 * - RF signal detection in spectrum monitoring
 * - Radar signal processing
 * - Communications signal identification
 * - Interference detection and classification
 *
 * Dependencies: stdlib.h, stdbool.h, stdint.h, math.h
 * Thread Safety: Not thread-safe (single detector instance per thread)
 * Performance: O(N log N) per FFT frame, suitable for real-time SDR
 */

#ifndef IQ_LAB_CFAR_OS_H
#define IQ_LAB_CFAR_OS_H

#include <stdint.h>
#include <stdbool.h>

// OS-CFAR detector configuration and state
typedef struct {
    // Configuration parameters
    uint32_t fft_size;           // Size of FFT frame (must be power of 2)
    double pfa;                  // Probability of False Alarm (1e-6 to 1e-2)
    uint32_t ref_cells;          // Training cells per side (8-32 typical)
    uint32_t guard_cells;        // Guard cells around CUT (1-4 typical)
    uint32_t os_rank;            // OS-CFAR rank parameter k (1 to ref_cells)

    // Derived parameters
    uint32_t total_training_cells;  // Total training cells (2 * ref_cells)
    double cfar_constant;        // Scaling constant for threshold

    // Working buffers
    double *training_buffer;     // Buffer for training cell values
    uint32_t buffer_size;        // Size of training buffer

    // State
    bool initialized;            // Whether detector is properly initialized
} cfar_os_t;

// Detection result structure
typedef struct {
    uint32_t bin_index;          // FFT bin where detection occurred
    double threshold;            // Adaptive threshold value (dB)
    double signal_power;         // Signal power at detection (dB)
    double snr_estimate;         // Estimated SNR (dB)
    double confidence;           // Detection confidence (0.0 to 1.0)
} cfar_detection_t;

// Public API functions

/*
 * Initialize OS-CFAR detector with specified parameters
 *
 * Parameters:
 *   cfar        - Pointer to detector structure to initialize
 *   fft_size    - Size of FFT frame (must be power of 2)
 *   pfa         - Desired Probability of False Alarm (e.g., 1e-3)
 *   ref_cells   - Number of training cells per side of CUT (8-32)
 *   guard_cells - Number of guard cells around CUT (1-4)
 *   os_rank     - OS-CFAR rank parameter k (1 to ref_cells)
 *
 * Returns:
 *   true on success, false on parameter error or allocation failure
 */
bool cfar_os_init(cfar_os_t *cfar, uint32_t fft_size, double pfa,
                  uint32_t ref_cells, uint32_t guard_cells, uint32_t os_rank);

/*
 * Process a single FFT frame and detect signals
 *
 * Parameters:
 *   cfar           - Pointer to initialized detector
 *   power_spectrum - FFT power spectrum in linear scale (size = fft_size)
 *   detections     - Output array to store detection results
 *   max_detections - Maximum number of detections to return
 *
 * Returns:
 *   Number of detections found (0 to max_detections)
 */
uint32_t cfar_os_process_frame(cfar_os_t *cfar, const double *power_spectrum,
                               cfar_detection_t *detections, uint32_t max_detections);

/*
 * Calculate the adaptive threshold for a specific CUT
 *
 * Parameters:
 *   cfar           - Pointer to initialized detector
 *   power_spectrum - FFT power spectrum in linear scale
 *   cut_index      - Index of Cell Under Test (0 to fft_size-1)
 *
 * Returns:
 *   Adaptive threshold value in linear scale, or 0.0 on error
 */
double cfar_os_get_threshold(cfar_os_t *cfar, const double *power_spectrum, uint32_t cut_index);

/*
 * Reset detector state (clear any internal buffers/state)
 *
 * Parameters:
 *   cfar - Pointer to detector to reset
 */
void cfar_os_reset(cfar_os_t *cfar);

/*
 * Free detector resources and reset to uninitialized state
 *
 * Parameters:
 *   cfar - Pointer to detector to free
 */
void cfar_os_free(cfar_os_t *cfar);

/*
 * Get current detector configuration as human-readable string
 *
 * Parameters:
 *   cfar - Pointer to detector
 *   buffer - Output buffer for configuration string
 *   buffer_size - Size of output buffer
 *
 * Returns:
 *   true on success, false if buffer too small
 */
bool cfar_os_get_config_string(const cfar_os_t *cfar, char *buffer, uint32_t buffer_size);

/*
 * Validate detector parameters without creating detector
 *
 * Parameters:
 *   fft_size    - Size of FFT frame
 *   pfa         - Desired Probability of False Alarm
 *   ref_cells   - Number of training cells per side
 *   guard_cells - Number of guard cells around CUT
 *   os_rank     - OS-CFAR rank parameter k
 *
 * Returns:
 *   true if parameters are valid, false otherwise
 */
bool cfar_os_validate_params(uint32_t fft_size, double pfa, uint32_t ref_cells,
                             uint32_t guard_cells, uint32_t os_rank);

#endif /* IQ_LAB_CFAR_OS_H */
