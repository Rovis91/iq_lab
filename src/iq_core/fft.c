#include "fft.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Radix-2 FFT implementation using Cooley-Tukey algorithm
 * Optimized for real-time signal processing applications
 */

// Internal helper functions
static uint32_t log2_uint32(uint32_t n);
static void fft_radix2(fft_complex_t *data, uint32_t size, fft_direction_t direction);
static bool create_bit_reversal_table(uint32_t *table, uint32_t size);
static bool create_twiddle_factors(fft_complex_t *twiddles, uint32_t size, fft_direction_t direction);

// Create FFT plan
fft_plan_t *fft_plan_create(uint32_t size, fft_direction_t direction) {
    if (!fft_is_power_of_two(size) || size == 0 || size > FFT_MAX_SIZE) {
        return NULL;
    }

    fft_plan_t *plan = (fft_plan_t *)malloc(sizeof(fft_plan_t));
    if (!plan) {
        return NULL;
    }

    plan->size = size;
    plan->direction = direction;
    plan->is_inverse_normalized = (direction == FFT_INVERSE);

    // Allocate twiddle factors
    uint32_t num_twiddles = size / 2;
    plan->twiddle_factors = (fft_complex_t *)malloc(num_twiddles * sizeof(fft_complex_t));
    if (!plan->twiddle_factors) {
        free(plan);
        return NULL;
    }

    // Allocate bit reversal table
    plan->bit_reversal_table = (uint32_t *)malloc(size * sizeof(uint32_t));
    if (!plan->bit_reversal_table) {
        free(plan->twiddle_factors);
        free(plan);
        return NULL;
    }

    // Create bit reversal table
    if (!create_bit_reversal_table(plan->bit_reversal_table, size)) {
        free(plan->bit_reversal_table);
        free(plan->twiddle_factors);
        free(plan);
        return NULL;
    }

    // Create twiddle factors
    if (!create_twiddle_factors(plan->twiddle_factors, size, direction)) {
        free(plan->bit_reversal_table);
        free(plan->twiddle_factors);
        free(plan);
        return NULL;
    }

    return plan;
}

// Destroy FFT plan
void fft_plan_destroy(fft_plan_t *plan) {
    if (!plan) return;

    if (plan->twiddle_factors) {
        free(plan->twiddle_factors);
        plan->twiddle_factors = NULL;
    }

    if (plan->bit_reversal_table) {
        free(plan->bit_reversal_table);
        plan->bit_reversal_table = NULL;
    }

    free(plan);
}

// Execute FFT using pre-computed plan
bool fft_execute(const fft_plan_t *plan, const fft_complex_t *input, fft_complex_t *output) {
    if (!plan || !input || !output) {
        return false;
    }

    // Copy input to output (we'll transform in-place)
    memcpy(output, input, plan->size * sizeof(fft_complex_t));

    // Apply bit reversal
    for (uint32_t i = 0; i < plan->size; i++) {
        uint32_t j = plan->bit_reversal_table[i];
        if (i < j) {
            // Swap elements
            fft_complex_t temp = output[i];
            output[i] = output[j];
            output[j] = temp;
        }
    }

    // Execute radix-2 FFT
    fft_radix2(output, plan->size, plan->direction);

    // Normalize inverse FFT
    if (plan->is_inverse_normalized) {
        double scale = 1.0 / (double)plan->size;
        for (uint32_t i = 0; i < plan->size; i++) {
            output[i] *= scale;
        }
    }

    return true;
}

// Convenience function for forward FFT
bool fft_forward(const fft_complex_t *input, fft_complex_t *output, uint32_t size) {
    fft_plan_t *plan = fft_plan_create(size, FFT_FORWARD);
    if (!plan) return false;

    bool success = fft_execute(plan, input, output);
    fft_plan_destroy(plan);

    return success;
}

// Convenience function for inverse FFT
bool fft_inverse(const fft_complex_t *input, fft_complex_t *output, uint32_t size) {
    fft_plan_t *plan = fft_plan_create(size, FFT_INVERSE);
    if (!plan) return false;

    bool success = fft_execute(plan, input, output);
    fft_plan_destroy(plan);

    return success;
}

// Real-to-complex FFT (for real-valued signals)
bool fft_real_forward(const double *input, fft_complex_t *output, uint32_t size) {
    if (!input || !output || size == 0) return false;

    // For now, just copy real input to complex output with zero imaginary parts
    // A more optimized version would use real FFT algorithms
    for (uint32_t i = 0; i < size; i++) {
        output[i] = input[i] + 0.0 * I;
    }

    return fft_forward(output, output, size);
}

// Complex-to-real inverse FFT
bool fft_real_inverse(const fft_complex_t *input, double *output, uint32_t size) {
    if (!input || !output || size == 0) return false;

    fft_complex_t *temp = (fft_complex_t *)malloc(size * sizeof(fft_complex_t));
    if (!temp) return false;

    bool success = fft_inverse(input, temp, size);
    if (success) {
        // Extract real parts
        for (uint32_t i = 0; i < size; i++) {
            output[i] = creal(temp[i]);
        }
    }

    free(temp);
    return success;
}

// Check if number is power of two
bool fft_is_power_of_two(uint32_t n) {
    return (n != 0) && ((n & (n - 1)) == 0);
}

// Get next power of two >= n
uint32_t fft_next_power_of_two(uint32_t n) {
    if (n == 0) return 1;

    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;

    return n;
}

// Bit reversal function
uint32_t fft_bit_reverse(uint32_t n, uint32_t bits) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < bits; i++) {
        result = (result << 1) | (n & 1);
        n >>= 1;
    }
    return result;
}

// Magnitude calculation
double fft_magnitude(fft_complex_t c) {
    return cabs(c);
}

// Magnitude squared (more efficient than magnitude for comparisons)
double fft_magnitude_squared(fft_complex_t c) {
    return creal(c) * creal(c) + cimag(c) * cimag(c);
}

// Phase calculation
double fft_phase(fft_complex_t c) {
    return carg(c);
}

// Magnitude in dB
double fft_magnitude_db(fft_complex_t c, double reference) {
    double mag = fft_magnitude(c);
    if (mag <= 0.0) return -INFINITY;
    return 20.0 * log10(mag / reference);
}

// Power spectrum calculation
bool fft_power_spectrum(const fft_complex_t *fft_output, double *power_spectrum,
                       uint32_t size, bool normalize) {
    if (!fft_output || !power_spectrum || size == 0) return false;

    double norm_factor = normalize ? (1.0 / (double)size) : 1.0;

    for (uint32_t i = 0; i < size; i++) {
        power_spectrum[i] = fft_magnitude_squared(fft_output[i]) * norm_factor;
    }

    return true;
}

// FFT shift: move DC component to center of spectrum
// This makes negative frequencies appear on the left, DC in middle, positive on right
bool fft_shift(const fft_complex_t *input, fft_complex_t *output, uint32_t size) {
    if (!input || !output || size == 0 || size % 2 != 0) return false;

    uint32_t half_size = size / 2;

    // Copy second half to beginning of output (negative frequencies)
    for (uint32_t i = 0; i < half_size; i++) {
        output[i] = input[i + half_size];
    }

    // Copy first half to end of output (positive frequencies + DC)
    for (uint32_t i = 0; i < half_size; i++) {
        output[i + half_size] = input[i];
    }

    return true;
}

// FFT shift for real arrays (power spectrum)
bool fft_shift_real(const double *input, double *output, uint32_t size) {
    if (!input || !output || size == 0 || size % 2 != 0) return false;

    uint32_t half_size = size / 2;

    // Copy second half to beginning of output (negative frequencies)
    for (uint32_t i = 0; i < half_size; i++) {
        output[i] = input[i + half_size];
    }

    // Copy first half to end of output (positive frequencies + DC)
    for (uint32_t i = 0; i < half_size; i++) {
        output[i + half_size] = input[i];
    }

    return true;
}

// Convert IQ samples to complex format
void fft_iq_to_complex(const float *iq_data, fft_complex_t *complex_data,
                      uint32_t num_samples, bool scale_to_unit) {
    if (!iq_data || !complex_data || num_samples == 0) return;

    double scale = scale_to_unit ? 1.0 : (1.0 / 32768.0);  // Assume s16 range

    for (uint32_t i = 0; i < num_samples; i++) {
        double i_val = (double)iq_data[i * 2] * scale;
        double q_val = (double)iq_data[i * 2 + 1] * scale;
        complex_data[i] = i_val + q_val * I;
    }
}

// Convert complex format back to IQ samples
void fft_complex_to_iq(const fft_complex_t *complex_data, float *iq_data,
                      uint32_t num_samples, bool scale_from_unit) {
    if (!complex_data || !iq_data || num_samples == 0) return;

    double scale = scale_from_unit ? 32767.0 : 1.0;  // Assume s16 range

    for (uint32_t i = 0; i < num_samples; i++) {
        iq_data[i * 2] = (float)(creal(complex_data[i]) * scale);
        iq_data[i * 2 + 1] = (float)(cimag(complex_data[i]) * scale);
    }
}

// Internal: Calculate log2 of uint32
static uint32_t log2_uint32(uint32_t n) {
    uint32_t log = 0;
    while (n >>= 1) log++;
    return log;
}

// Internal: Create bit reversal table
static bool create_bit_reversal_table(uint32_t *table, uint32_t size) {
    if (!table || size == 0) return false;

    uint32_t bits = log2_uint32(size);

    for (uint32_t i = 0; i < size; i++) {
        table[i] = fft_bit_reverse(i, bits);
    }

    return true;
}

// Internal: Create twiddle factors
static bool create_twiddle_factors(fft_complex_t *twiddles, uint32_t size, fft_direction_t direction) {
    if (!twiddles || size == 0) return false;

    uint32_t num_twiddles = size / 2;
    double angle_scale = (direction == FFT_FORWARD) ? -2.0 * M_PI : 2.0 * M_PI;

    for (uint32_t i = 0; i < num_twiddles; i++) {
        double angle = angle_scale * (double)i / (double)size;
        twiddles[i] = cos(angle) + sin(angle) * I;
    }

    return true;
}

// Internal: Radix-2 FFT implementation
static void fft_radix2(fft_complex_t *data, uint32_t size, fft_direction_t direction) {
    uint32_t stages = log2_uint32(size);

    // For each stage
    for (uint32_t stage = 1; stage <= stages; stage++) {
        uint32_t half_size = 1 << (stage - 1);  // 2^(stage-1)
        uint32_t full_size = 1 << stage;        // 2^stage

        // For each sub-FFT
        for (uint32_t group = 0; group < size; group += full_size) {
            // For each butterfly in the sub-FFT
            for (uint32_t k = 0; k < half_size; k++) {
                uint32_t index1 = group + k;
                uint32_t index2 = group + k + half_size;

                // Twiddle factor
                double angle = (direction == FFT_FORWARD) ?
                    -2.0 * M_PI * (double)k / (double)full_size :
                     2.0 * M_PI * (double)k / (double)full_size;

                fft_complex_t twiddle = cos(angle) + sin(angle) * I;

                // Butterfly operation
                fft_complex_t temp = data[index2] * twiddle;
                data[index2] = data[index1] - temp;
                data[index1] = data[index1] + temp;
            }
        }
    }
}
