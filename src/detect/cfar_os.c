/*
 * IQ Lab - OS-CFAR Detector Implementation
 *
 * This file implements the Ordered Statistics Constant False Alarm Rate (OS-CFAR)
 * algorithm for detecting signals in frequency domain power spectra. The algorithm
 * uses ordered statistics to provide robust detection in non-homogeneous noise
 * environments where traditional CFAR methods may fail.
 *
 * Algorithm Details:
 * 1. For each Cell Under Test (CUT):
 *    - Select training cells from sliding window around CUT
 *    - Exclude guard cells immediately adjacent to CUT
 *    - Sort training cell values in ascending order
 *    - Select the k-th highest value (OS rank parameter)
 *    - Scale by CFAR constant to achieve desired PFA
 *    - Compare CUT against adaptive threshold
 *
 * CFAR Constant Calculation:
 * The CFAR constant is derived from ordered statistics theory to achieve the
 * desired Probability of False Alarm. For OS-CFAR, it's based on the binomial
 * distribution and the desired PFA target.
 *
 * Performance Considerations:
 * - Sorting dominates complexity: O(N log N) per frame
 * - Memory usage is O(FFT_SIZE) for training buffer
 * - Suitable for real-time processing up to ~8192 FFT size
 *
 * Edge Cases Handled:
 * - Boundary conditions at spectrum edges
 * - Insufficient training cells near band edges
 * - Zero or negative power values
 * - Parameter validation and error recovery
 */

#include "cfar_os.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// Internal helper functions

/*
 * Calculate CFAR constant for desired PFA using ordered statistics
 * This is derived from the theory of ordered statistics and binomial distribution
 */
static double calculate_cfar_constant(uint32_t num_training_cells, uint32_t os_rank, double pfa) {
    // For OS-CFAR, the threshold multiplier is based on ordered statistics
    // This is a simplified approximation - in practice, this would be
    // calculated using the inverse of the binomial cumulative distribution

    if (num_training_cells == 0 || os_rank == 0 || pfa <= 0.0) {
        return 1.0;
    }

    // Simplified CFAR constant calculation
    // This provides reasonable performance for typical PFA values (1e-6 to 1e-2)
    double alpha = log(1.0 / pfa) / (double)num_training_cells;

    // OS-CFAR scaling factor based on rank parameter
    double os_factor = 1.0 + (double)(num_training_cells - os_rank) / (double)os_rank;

    return alpha * os_factor;
}

/*
 * Quickselect algorithm for finding k-th smallest element
 * Used for ordered statistics computation (faster than full sort)
 */
static double quickselect(double *arr, int n, int k) {
    if (n <= 0 || k < 0 || k >= n) {
        return 0.0;
    }

    int left = 0, right = n - 1;

    while (left <= right) {
        int pivot_index = left + (right - left) / 2;
        double pivot_value = arr[pivot_index];

        // Partition around pivot
        int i = left, j = right;
        while (i <= j) {
            while (arr[i] < pivot_value) i++;
            while (arr[j] > pivot_value) j--;
            if (i <= j) {
                // Swap
                double temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
                i++;
                j--;
            }
        }

        // Now arr[left..j] <= pivot_value <= arr[i..right]
        if (k <= j) {
            right = j;
        } else if (k >= i) {
            left = i;
        } else {
            return pivot_value;
        }
    }

    return arr[k];
}


// Public API implementation

bool cfar_os_init(cfar_os_t *cfar, uint32_t fft_size, double pfa,
                  uint32_t ref_cells, uint32_t guard_cells, uint32_t os_rank) {

    if (!cfar || fft_size == 0) {
        return false;
    }

    // Validate parameters
    if (!cfar_os_validate_params(fft_size, pfa, ref_cells, guard_cells, os_rank)) {
        return false;
    }

    // Initialize basic parameters
    cfar->fft_size = fft_size;
    cfar->pfa = pfa;
    cfar->ref_cells = ref_cells;
    cfar->guard_cells = guard_cells;
    cfar->os_rank = os_rank;
    cfar->total_training_cells = 2 * ref_cells;

    // Allocate training buffer
    cfar->buffer_size = cfar->total_training_cells;
    cfar->training_buffer = malloc(cfar->buffer_size * sizeof(double));
    if (!cfar->training_buffer) {
        return false;
    }

    // Calculate CFAR constant for desired PFA
    cfar->cfar_constant = calculate_cfar_constant(cfar->total_training_cells, os_rank, pfa);

    cfar->initialized = true;
    return true;
}

uint32_t cfar_os_process_frame(cfar_os_t *cfar, const double *power_spectrum,
                               cfar_detection_t *detections, uint32_t max_detections) {

    if (!cfar || !cfar->initialized || !power_spectrum || !detections || max_detections == 0) {
        return 0;
    }

    uint32_t num_detections = 0;

    // Process each bin as a potential Cell Under Test (CUT)
    for (uint32_t cut_index = 0; cut_index < cfar->fft_size && num_detections < max_detections; cut_index++) {

        // Calculate adaptive threshold for this CUT
        double threshold_linear = cfar_os_get_threshold(cfar, power_spectrum, cut_index);
        if (threshold_linear <= 0.0) {
            continue; // Skip if threshold calculation failed
        }

        // Get signal power at CUT
        double signal_power_linear = power_spectrum[cut_index];
        if (signal_power_linear <= 0.0) {
            continue; // Skip invalid power values
        }

        // Convert to dB for easier interpretation
        double threshold_db = 10.0 * log10(threshold_linear);
        double signal_power_db = 10.0 * log10(signal_power_linear);

        // Check if signal exceeds threshold
        if (signal_power_linear > threshold_linear) {
            // Detection found!
            cfar_detection_t *detection = &detections[num_detections];

            detection->bin_index = cut_index;
            detection->threshold = threshold_db;
            detection->signal_power = signal_power_db;
            detection->snr_estimate = signal_power_db - threshold_db;

            // Calculate confidence based on SNR
            double snr_ratio = signal_power_linear / threshold_linear;
            detection->confidence = fmin(1.0, snr_ratio / 10.0); // Cap at 10dB SNR

            num_detections++;
        }
    }

    return num_detections;
}

double cfar_os_get_threshold(cfar_os_t *cfar, const double *power_spectrum, uint32_t cut_index) {

    if (!cfar || !cfar->initialized || !power_spectrum || cut_index >= cfar->fft_size) {
        return 0.0;
    }

    // Calculate training window boundaries
    int32_t window_start = (int32_t)cut_index - (int32_t)cfar->guard_cells - (int32_t)cfar->ref_cells;
    int32_t window_end = (int32_t)cut_index + (int32_t)cfar->guard_cells + (int32_t)cfar->ref_cells + 1;

    // Clamp to spectrum boundaries
    if (window_start < 0) window_start = 0;
    if ((uint32_t)window_end > cfar->fft_size) window_end = (int32_t)cfar->fft_size;

    // Extract training cells (exclude CUT and guard cells)
    uint32_t training_count = 0;

    for (int32_t i = window_start; i < window_end && training_count < cfar->buffer_size; i++) {
        // Skip CUT and guard cells around it
        int32_t distance_from_cut = abs((int32_t)i - (int32_t)cut_index);
        if (distance_from_cut <= (int32_t)cfar->guard_cells) {
            continue; // Skip guard cells and CUT
        }

        // Add to training buffer
        double power_val = power_spectrum[i];
        if (power_val > 0.0) { // Only include valid power values
            cfar->training_buffer[training_count++] = power_val;
        }
    }

    // Check if we have enough training cells
    if (training_count < cfar->os_rank) {
        return 0.0; // Insufficient training cells
    }

    // Use quickselect to find the k-th smallest element (OS-CFAR)
    // For OS-CFAR, we want the k-th highest, so we select the (N-k+1)-th smallest
    uint32_t select_index = training_count - cfar->os_rank;
    double os_value = quickselect(cfar->training_buffer, training_count, select_index);

    // Apply CFAR constant to get final threshold
    double threshold = os_value * cfar->cfar_constant;

    return threshold;
}

void cfar_os_reset(cfar_os_t *cfar) {
    if (!cfar) return;

    // Clear training buffer if allocated
    if (cfar->training_buffer) {
        memset(cfar->training_buffer, 0, cfar->buffer_size * sizeof(double));
    }

    // Reset derived parameters (keep configuration)
    cfar->total_training_cells = 2 * cfar->ref_cells;
    cfar->cfar_constant = calculate_cfar_constant(cfar->total_training_cells,
                                                 cfar->os_rank, cfar->pfa);
}

void cfar_os_free(cfar_os_t *cfar) {
    if (!cfar) return;

    // Free training buffer
    if (cfar->training_buffer) {
        free(cfar->training_buffer);
        cfar->training_buffer = NULL;
    }

    // Reset all fields
    memset(cfar, 0, sizeof(cfar_os_t));
    cfar->initialized = false;
}

bool cfar_os_get_config_string(const cfar_os_t *cfar, char *buffer, uint32_t buffer_size) {
    if (!cfar || !buffer || buffer_size == 0) {
        return false;
    }

    int written = snprintf(buffer, buffer_size,
                          "OS-CFAR: FFT=%u, PFA=%.2e, RefCells=%u, GuardCells=%u, OS-Rank=%u",
                          cfar->fft_size, cfar->pfa, cfar->ref_cells,
                          cfar->guard_cells, cfar->os_rank);

    return (written > 0 && (uint32_t)written < buffer_size);
}

bool cfar_os_validate_params(uint32_t fft_size, double pfa, uint32_t ref_cells,
                             uint32_t guard_cells, uint32_t os_rank) {

    // Basic parameter validation
    if (fft_size == 0 || fft_size > 65536) { // Reasonable upper limit
        return false;
    }

    if (pfa <= 0.0 || pfa > 1.0) {
        return false;
    }

    if (ref_cells == 0 || ref_cells > fft_size / 4) { // Can't use more than 1/4 of spectrum
        return false;
    }

    if (guard_cells > ref_cells) { // Guard cells shouldn't exceed reference cells
        return false;
    }

    if (os_rank == 0 || os_rank > ref_cells) {
        return false;
    }

    // Check that we have enough spectrum for the training window
    uint32_t min_spectrum_needed = 2 * (ref_cells + guard_cells) + 1;
    if (fft_size < min_spectrum_needed) {
        return false;
    }

    return true;
}
