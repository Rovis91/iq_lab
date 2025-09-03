#ifndef IQ_FFT_H
#define IQ_FFT_H

#include <stdint.h>
#include <stdbool.h>
#include <complex.h>

// FFT configuration
#define FFT_MAX_SIZE 1048576  // 2^20, should be enough for most applications

/*
 * Complex number type for FFT operations
 * Using C99 complex numbers for better portability
 */
typedef double complex fft_complex_t;

/*
 * FFT direction
 */
typedef enum {
    FFT_FORWARD = -1,   // Forward FFT (time -> frequency)
    FFT_INVERSE = 1    // Inverse FFT (frequency -> time)
} fft_direction_t;

/*
 * FFT plan structure
 * Contains pre-computed twiddle factors and bit reversal table
 */
typedef struct {
    uint32_t size;                    // FFT size (must be power of 2)
    fft_direction_t direction;        // Forward or inverse
    fft_complex_t *twiddle_factors;   // Pre-computed twiddle factors
    uint32_t *bit_reversal_table;     // Bit reversal lookup table
    bool is_inverse_normalized;       // Whether to normalize inverse FFT
} fft_plan_t;

/*
 * Function declarations
 */

// Initialize FFT plan for given size and direction
fft_plan_t *fft_plan_create(uint32_t size, fft_direction_t direction);

// Destroy FFT plan and free memory
void fft_plan_destroy(fft_plan_t *plan);

// Execute FFT using the plan
bool fft_execute(const fft_plan_t *plan, const fft_complex_t *input, fft_complex_t *output);

// Convenience functions for one-off FFTs (creates/destroys plan internally)
bool fft_forward(const fft_complex_t *input, fft_complex_t *output, uint32_t size);
bool fft_inverse(const fft_complex_t *input, fft_complex_t *output, uint32_t size);

// Real-to-complex FFT (for real-valued input signals)
bool fft_real_forward(const double *input, fft_complex_t *output, uint32_t size);

// Complex-to-real inverse FFT (for complex spectrum to real signal)
bool fft_real_inverse(const fft_complex_t *input, double *output, uint32_t size);

// Utility functions
bool fft_is_power_of_two(uint32_t n);
uint32_t fft_next_power_of_two(uint32_t n);
uint32_t fft_bit_reverse(uint32_t n, uint32_t bits);

// Magnitude and phase calculation
double fft_magnitude(fft_complex_t c);
double fft_magnitude_squared(fft_complex_t c);
double fft_phase(fft_complex_t c);
double fft_magnitude_db(fft_complex_t c, double reference);

// Power spectrum calculation
bool fft_power_spectrum(const fft_complex_t *fft_output, double *power_spectrum,
                       uint32_t size, bool normalize);

// FFT shift: move DC component to center of spectrum
bool fft_shift(const fft_complex_t *input, fft_complex_t *output, uint32_t size);

// FFT shift for real arrays (power spectrum)
bool fft_shift_real(const double *input, double *output, uint32_t size);

// Convert real IQ samples to complex format
void fft_iq_to_complex(const float *iq_data, fft_complex_t *complex_data,
                      uint32_t num_samples, bool scale_to_unit);

// Convert complex format back to real IQ samples
void fft_complex_to_iq(const fft_complex_t *complex_data, float *iq_data,
                      uint32_t num_samples, bool scale_from_unit);

#endif // IQ_FFT_H
