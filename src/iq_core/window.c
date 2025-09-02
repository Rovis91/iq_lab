#include "window.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <complex.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Window function implementations
 * Based on standard DSP window formulations
 */

// Internal window coefficient generators
static bool generate_rectangular(double *coeffs, uint32_t size);
static bool generate_hann(double *coeffs, uint32_t size);
static bool generate_hamming(double *coeffs, uint32_t size);
static bool generate_blackman(double *coeffs, uint32_t size);
static bool generate_blackman_harris(double *coeffs, uint32_t size);
static bool generate_flat_top(double *coeffs, uint32_t size);

// Window property calculators
static double calculate_coherent_gain(const double *coeffs, uint32_t size);
static double calculate_processing_gain(const double *coeffs, uint32_t size);
// static double calculate_amplitude_correction(double coherent_gain);  // Commented out
// static double calculate_power_correction(double coherent_gain);      // Commented out
static double calculate_enbw_bins(const double *coeffs, uint32_t size);
static double calculate_scalloping_loss_db(window_type_t type);

// Create window function
window_t *window_create(window_type_t type, uint32_t size) {
    if (size == 0) {
        return NULL;
    }

    window_t *window = (window_t *)malloc(sizeof(window_t));
    if (!window) {
        return NULL;
    }

    window->type = type;
    window->size = size;
    window->coefficients = (double *)malloc(size * sizeof(double));
    if (!window->coefficients) {
        free(window);
        return NULL;
    }

    // Generate window coefficients
    bool success = false;
    switch (type) {
        case WINDOW_RECTANGULAR:
            success = generate_rectangular(window->coefficients, size);
            strcpy(window->name, "Rectangular");
            break;
        case WINDOW_HANN:
            success = generate_hann(window->coefficients, size);
            strcpy(window->name, "Hann");
            break;
        case WINDOW_HAMMING:
            success = generate_hamming(window->coefficients, size);
            strcpy(window->name, "Hamming");
            break;
        case WINDOW_BLACKMAN:
            success = generate_blackman(window->coefficients, size);
            strcpy(window->name, "Blackman");
            break;
        case WINDOW_BLACKMAN_HARRIS:
            success = generate_blackman_harris(window->coefficients, size);
            strcpy(window->name, "Blackman-Harris");
            break;
        case WINDOW_FLAT_TOP:
            success = generate_flat_top(window->coefficients, size);
            strcpy(window->name, "Flat-Top");
            break;
        default:
            success = false;
            break;
    }

    if (!success) {
        free(window->coefficients);
        free(window);
        return NULL;
    }

    // Calculate normalization factors
    window->normalization_factor = calculate_coherent_gain(window->coefficients, size);

    return window;
}

// Destroy window function
void window_destroy(window_t *window) {
    if (!window) return;

    if (window->coefficients) {
        free(window->coefficients);
        window->coefficients = NULL;
    }

    free(window);
}

// Apply window to real-valued signal
bool window_apply_real(const window_t *window, const double *input, double *output) {
    if (!window || !input || !output) {
        return false;
    }

    for (uint32_t i = 0; i < window->size; i++) {
        output[i] = input[i] * window->coefficients[i];
    }

    return true;
}

// Apply window to complex-valued signal
bool window_apply_complex(const window_t *window,
                         const double complex *input,
                         double complex *output) {
    if (!window || !input || !output) {
        return false;
    }

    for (uint32_t i = 0; i < window->size; i++) {
        output[i] = input[i] * window->coefficients[i];
    }

    return true;
}

// Get window coefficient at specific index
double window_coefficient(const window_t *window, uint32_t index) {
    if (!window || index >= window->size) {
        return 0.0;
    }

    return window->coefficients[index];
}

// Get window normalization factor for amplitude correction
double window_amplitude_correction(const window_t *window) {
    if (!window) return 1.0;
    return 1.0 / window->normalization_factor;
}

// Get window normalization factor for power correction
double window_power_correction(const window_t *window) {
    if (!window) return 1.0;
    double coherent_gain = window->normalization_factor;
    return 1.0 / (coherent_gain * coherent_gain);
}

// Get window equivalent noise bandwidth (ENBW) in bins
double window_enbw_bins(const window_t *window) {
    if (!window) return 1.0;
    return calculate_enbw_bins(window->coefficients, window->size);
}

// Get window scalloping loss in dB
double window_scalloping_loss_db(const window_t *window) {
    if (!window) return 0.0;
    return calculate_scalloping_loss_db(window->type);
}

// Create window from name string
window_t *window_create_from_name(const char *name, uint32_t size) {
    if (!name) return NULL;

    window_type_t type = WINDOW_HANN; // Default

    if (strcmp(name, "rectangular") == 0 || strcmp(name, "boxcar") == 0) {
        type = WINDOW_RECTANGULAR;
    } else if (strcmp(name, "hann") == 0 || strcmp(name, "hanning") == 0) {
        type = WINDOW_HANN;
    } else if (strcmp(name, "hamming") == 0) {
        type = WINDOW_HAMMING;
    } else if (strcmp(name, "blackman") == 0) {
        type = WINDOW_BLACKMAN;
    } else if (strcmp(name, "blackman-harris") == 0 || strcmp(name, "bh") == 0) {
        type = WINDOW_BLACKMAN_HARRIS;
    } else if (strcmp(name, "flat-top") == 0 || strcmp(name, "flattop") == 0) {
        type = WINDOW_FLAT_TOP;
    }

    return window_create(type, size);
}

// Get list of available window names
const char **window_get_available_names(void) {
    static const char *names[] = {
        "rectangular",
        "hann",
        "hamming",
        "blackman",
        "blackman-harris",
        "flat-top",
        NULL
    };
    return names;
}

// Get human-readable description of window
const char *window_get_description(window_type_t type) {
    switch (type) {
        case WINDOW_RECTANGULAR:
            return "Rectangular (no windowing) - Maximum frequency resolution, high spectral leakage";
        case WINDOW_HANN:
            return "Hann window - Good balance of resolution and sidelobe suppression";
        case WINDOW_HAMMING:
            return "Hamming window - Slightly better resolution than Hann, higher sidelobes";
        case WINDOW_BLACKMAN:
            return "Blackman window - Excellent sidelobe suppression, wider main lobe";
        case WINDOW_BLACKMAN_HARRIS:
            return "Blackman-Harris window - Superior sidelobe suppression for high dynamic range";
        case WINDOW_FLAT_TOP:
            return "Flat-top window - Maximum amplitude accuracy, poorest frequency resolution";
        default:
            return "Unknown window type";
    }
}

// Utility functions
double window_coherent_gain(const window_t *window) {
    return window ? window->normalization_factor : 1.0;
}

double window_processing_gain(const window_t *window) {
    if (!window) return 0.0;
    return calculate_processing_gain(window->coefficients, window->size);
}

// Internal window coefficient generators
static bool generate_rectangular(double *coeffs, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        coeffs[i] = 1.0;
    }
    return true;
}

static bool generate_hann(double *coeffs, uint32_t size) {
    if (size == 1) {
        coeffs[0] = 1.0;  // Special case for size 1
        return true;
    }

    for (uint32_t i = 0; i < size; i++) {
        double phase = 2.0 * M_PI * (double)i / (double)(size - 1);
        coeffs[i] = 0.5 * (1.0 - cos(phase));
    }
    return true;
}

static bool generate_hamming(double *coeffs, uint32_t size) {
    if (size == 1) {
        coeffs[0] = 1.0;  // Special case for size 1
        return true;
    }

    for (uint32_t i = 0; i < size; i++) {
        double phase = 2.0 * M_PI * (double)i / (double)(size - 1);
        coeffs[i] = 0.54 - 0.46 * cos(phase);
    }
    return true;
}

static bool generate_blackman(double *coeffs, uint32_t size) {
    if (size == 1) {
        coeffs[0] = 1.0;  // Special case for size 1
        return true;
    }

    for (uint32_t i = 0; i < size; i++) {
        double phase = 2.0 * M_PI * (double)i / (double)(size - 1);
        coeffs[i] = 0.42 - 0.5 * cos(phase) + 0.08 * cos(2.0 * phase);
    }
    return true;
}

static bool generate_blackman_harris(double *coeffs, uint32_t size) {
    if (size == 1) {
        coeffs[0] = 1.0;  // Special case for size 1
        return true;
    }

    for (uint32_t i = 0; i < size; i++) {
        double phase = 2.0 * M_PI * (double)i / (double)(size - 1);
        coeffs[i] = 0.35875 - 0.48829 * cos(phase) +
                   0.14128 * cos(2.0 * phase) - 0.01168 * cos(3.0 * phase);
    }
    return true;
}

static bool generate_flat_top(double *coeffs, uint32_t size) {
    if (size == 1) {
        coeffs[0] = 1.0;  // Special case for size 1
        return true;
    }

    // 5-term flat-top window for high amplitude accuracy
    static const double a[5] = {0.21557895, 0.41663158, 0.277263158, 0.083578947, 0.006947368};

    for (uint32_t i = 0; i < size; i++) {
        double phase = 2.0 * M_PI * (double)i / (double)(size - 1);
        coeffs[i] = a[0] - a[1] * cos(phase) + a[2] * cos(2.0 * phase) -
                   a[3] * cos(3.0 * phase) + a[4] * cos(4.0 * phase);
    }
    return true;
}

// Window property calculators
static double calculate_coherent_gain(const double *coeffs, uint32_t size) {
    double sum = 0.0;
    for (uint32_t i = 0; i < size; i++) {
        sum += coeffs[i];
    }
    return sum / (double)size;
}

static double calculate_processing_gain(const double *coeffs, uint32_t size) {
    double sum_squares = 0.0;
    for (uint32_t i = 0; i < size; i++) {
        sum_squares += coeffs[i] * coeffs[i];
    }
    return 10.0 * log10((sum_squares / (double)size));
}

/*
static double calculate_amplitude_correction(double coherent_gain) {
    return 1.0 / coherent_gain;
}

static double calculate_power_correction(double coherent_gain) {
    return 1.0 / (coherent_gain * coherent_gain);
}
*/

static double calculate_enbw_bins(const double *coeffs, uint32_t size) {
    double sum_squares = 0.0;
    for (uint32_t i = 0; i < size; i++) {
        sum_squares += coeffs[i] * coeffs[i];
    }
    double enbw = (sum_squares / (double)size) * (double)size;
    return enbw;
}

static double calculate_scalloping_loss_db(window_type_t type) {
    // Approximate scalloping loss values (dB)
    switch (type) {
        case WINDOW_RECTANGULAR: return -3.922;  // Worst scalloping
        case WINDOW_HANN: return -1.424;         // Good balance
        case WINDOW_HAMMING: return -1.783;      // Slightly better than Hann
        case WINDOW_BLACKMAN: return -0.826;     // Good suppression
        case WINDOW_BLACKMAN_HARRIS: return -0.827; // Very good suppression
        case WINDOW_FLAT_TOP: return -0.014;     // Minimal scalloping
        default: return 0.0;
    }
}
