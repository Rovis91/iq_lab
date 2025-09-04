#ifndef IQ_LAB_RESAMPLE_H
#define IQ_LAB_RESAMPLE_H

#include <stdint.h>
#include <stdbool.h>

// Resampling context for rational rate conversion
typedef struct {
    // Conversion parameters
    uint32_t input_rate;     // Input sample rate (Hz)
    uint32_t output_rate;    // Output sample rate (Hz)
    double ratio;           // Conversion ratio (output_rate / input_rate)

    // Filter parameters
    uint32_t filter_length;  // Length of anti-aliasing filter
    float cutoff_freq;      // Cutoff frequency (0.0 to 0.5, normalized to output rate)
    float *filter_coeffs;   // FIR filter coefficients

    // Internal state
    double phase;           // Current phase accumulator (0.0 to 1.0)
    uint32_t input_count;   // Number of input samples processed
    uint32_t output_count;  // Number of output samples generated

    // History buffer for FIR filtering
    uint32_t history_length;
    float *history_buffer;
} resample_t;

// Initialize resampler with given rates
bool resample_init(resample_t *resampler, uint32_t input_rate, uint32_t output_rate);

// Initialize resampler with custom filter parameters
bool resample_init_custom(resample_t *resampler, uint32_t input_rate, uint32_t output_rate,
                         uint32_t filter_length, float cutoff_freq);

// Process a single input sample and generate output samples
uint32_t resample_process_sample(resample_t *resampler, float input_sample,
                                float *output_buffer, uint32_t max_output_samples);

// Process a buffer of input samples
bool resample_process_buffer(resample_t *resampler,
                           const float *input_buffer, uint32_t input_samples,
                           float *output_buffer, uint32_t max_output_samples,
                           uint32_t *output_samples_generated);

// Reset resampler state
void resample_reset(resample_t *resampler);

// Free resampler resources
void resample_free(resample_t *resampler);

// Utility functions
uint32_t resample_estimate_output_size(uint32_t input_samples, double ratio);
double resample_get_ratio(uint32_t input_rate, uint32_t output_rate);

// Predefined configurations for common conversions
bool resample_init_audio_48k(resample_t *resampler, uint32_t input_rate);  // To 48kHz audio
bool resample_init_audio_44k(resample_t *resampler, uint32_t input_rate);  // To 44.1kHz audio

#endif // IQ_LAB_RESAMPLE_H
