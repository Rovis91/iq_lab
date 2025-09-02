#ifndef IQ_WINDOW_H
#define IQ_WINDOW_H

#include <stdint.h>
#include <stdbool.h>
#include <complex.h>

/*
 * Window functions for FFT-based signal processing
 * Essential for reducing spectral leakage in spectrum analysis
 */

typedef enum {
    WINDOW_RECTANGULAR,    // No windowing (boxcar)
    WINDOW_HANN,          // Hann window (cosine taper)
    WINDOW_HAMMING,       // Hamming window
    WINDOW_BLACKMAN,      // Blackman window
    WINDOW_BLACKMAN_HARRIS, // Blackman-Harris window
    WINDOW_FLAT_TOP       // Flat-top window (high amplitude accuracy)
} window_type_t;

/*
 * Window function structure
 */
typedef struct {
    window_type_t type;        // Window type
    uint32_t size;            // Window size (must match FFT size)
    double *coefficients;     // Pre-computed window coefficients
    double normalization_factor; // For power/amplitude normalization
    char name[32];           // Human-readable name
} window_t;

/*
 * Function declarations
 */

// Create a window function
window_t *window_create(window_type_t type, uint32_t size);

// Destroy window function
void window_destroy(window_t *window);

// Apply window to real-valued signal
bool window_apply_real(const window_t *window, const double *input, double *output);

// Apply window to complex-valued signal
bool window_apply_complex(const window_t *window,
                         const double complex *input,
                         double complex *output);

// Get window coefficient at specific index
double window_coefficient(const window_t *window, uint32_t index);

// Get window normalization factor for amplitude correction
double window_amplitude_correction(const window_t *window);

// Get window normalization factor for power correction
double window_power_correction(const window_t *window);

// Get window equivalent noise bandwidth (ENBW) in bins
double window_enbw_bins(const window_t *window);

// Get window scalloping loss in dB
double window_scalloping_loss_db(const window_t *window);

// Create window from name string
window_t *window_create_from_name(const char *name, uint32_t size);

// Get list of available window names
const char **window_get_available_names(void);

// Get human-readable description of window
const char *window_get_description(window_type_t type);

// Utility functions
double window_coherent_gain(const window_t *window);
double window_processing_gain(const window_t *window);

#endif // IQ_WINDOW_H
