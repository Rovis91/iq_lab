#include "resample.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Initialize resampler with default parameters
bool resample_init(resample_t *resampler, uint32_t input_rate, uint32_t output_rate) {
    // Use reasonable defaults for audio resampling
    return resample_init_custom(resampler, input_rate, output_rate, 32, 0.4f);
}

// Initialize resampler with custom parameters
bool resample_init_custom(resample_t *resampler, uint32_t input_rate, uint32_t output_rate,
                         uint32_t filter_length, float cutoff_freq) {
    if (!resampler || input_rate == 0 || output_rate == 0 ||
        filter_length == 0 || cutoff_freq <= 0.0f || cutoff_freq >= 0.5f) {
        return false;
    }

    // Set basic parameters
    resampler->input_rate = input_rate;
    resampler->output_rate = output_rate;
    resampler->ratio = (double)output_rate / (double)input_rate;
    resampler->filter_length = filter_length;
    resampler->cutoff_freq = cutoff_freq;

    // Initialize state
    resampler->phase = 0.0;
    resampler->input_count = 0;
    resampler->output_count = 0;

    // Allocate history buffer (for filter state)
    resampler->history_length = filter_length;
    resampler->history_buffer = (float *)calloc(resampler->history_length, sizeof(float));
    if (!resampler->history_buffer) {
        return false;
    }

    // Allocate and generate filter coefficients (simple windowed sinc)
    resampler->filter_coeffs = (float *)malloc(filter_length * sizeof(float));
    if (!resampler->filter_coeffs) {
        free(resampler->history_buffer);
        resampler->history_buffer = NULL;
        return false;
    }

    // Generate windowed sinc filter coefficients
    // This is a simplified filter - in production you'd use a proper window function
    float sum = 0.0f;
    for (uint32_t i = 0; i < filter_length; i++) {
        float x = 2.0f * M_PI * cutoff_freq * (i - filter_length/2.0f);
        if (fabsf(x) < 1e-6f) {
            resampler->filter_coeffs[i] = 1.0f; // Handle sinc(0) = 1
        } else {
            resampler->filter_coeffs[i] = sinf(x) / x;
        }
        // Simple Hamming window
        float window = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (filter_length - 1));
        resampler->filter_coeffs[i] *= window;
        sum += fabsf(resampler->filter_coeffs[i]);
    }

    // Normalize filter coefficients
    if (sum > 0.0f) {
        for (uint32_t i = 0; i < filter_length; i++) {
            resampler->filter_coeffs[i] /= sum;
        }
    }

    return true;
}

// Process a single input sample and generate output samples
uint32_t resample_process_sample(resample_t *resampler, float input_sample,
                                float *output_buffer, uint32_t max_output_samples) {
    if (!resampler || !output_buffer || max_output_samples == 0) {
        return 0;
    }

    uint32_t samples_generated = 0;

    // Update history buffer
    if (resampler->history_buffer) {
        // Shift history
        for (uint32_t i = resampler->history_length - 1; i > 0; i--) {
            resampler->history_buffer[i] = resampler->history_buffer[i - 1];
        }
        resampler->history_buffer[0] = input_sample;
    }

    // Generate output samples based on conversion ratio
    // Use inverse ratio for phase increment
    double phase_increment = 1.0 / resampler->ratio;

    while (resampler->phase < 1.0 && samples_generated < max_output_samples) {
        // Linear interpolation between current and previous sample
        float current_sample = input_sample;
        float prev_sample = (resampler->history_length > 1) ?
                           resampler->history_buffer[1] : current_sample;

        // Interpolate based on phase
        float interpolated_sample = prev_sample +
                                  (current_sample - prev_sample) * (float)resampler->phase;

        // Apply simple low-pass filtering (if history available)
        if (resampler->filter_coeffs && resampler->history_buffer) {
            float filtered_sample = 0.0f;
            for (uint32_t i = 0; i < resampler->history_length; i++) {
                filtered_sample += resampler->history_buffer[i] * resampler->filter_coeffs[i];
            }
            interpolated_sample = filtered_sample;
        }

        output_buffer[samples_generated] = interpolated_sample;
        samples_generated++;

        // Advance phase by inverse ratio
        resampler->phase += phase_increment;
    }

    // Reset phase for next input sample (keep fractional part)
    resampler->phase = fmod(resampler->phase, 1.0);
    resampler->input_count++;
    resampler->output_count += samples_generated;

    return samples_generated;
}

// Process a buffer of input samples
bool resample_process_buffer(resample_t *resampler,
                           const float *input_buffer, uint32_t input_samples,
                           float *output_buffer, uint32_t max_output_samples,
                           uint32_t *output_samples_generated) {
    if (!resampler || !input_buffer || !output_buffer || !output_samples_generated ||
        input_samples == 0 || max_output_samples == 0) {
        return false;
    }

    uint32_t total_output = 0;
    uint32_t output_remaining = max_output_samples;

    for (uint32_t i = 0; i < input_samples && output_remaining > 0; i++) {
        uint32_t generated = resample_process_sample(resampler, input_buffer[i],
                                                   output_buffer + total_output, output_remaining);
        total_output += generated;
        output_remaining -= generated;
    }

    *output_samples_generated = total_output;
    return true;
}

// Reset resampler state
void resample_reset(resample_t *resampler) {
    if (!resampler) return;

    resampler->phase = 0.0;
    resampler->input_count = 0;
    resampler->output_count = 0;

    // Clear history buffer
    if (resampler->history_buffer) {
        memset(resampler->history_buffer, 0, resampler->history_length * sizeof(float));
    }
}

// Free resampler resources
void resample_free(resample_t *resampler) {
    if (!resampler) return;

    if (resampler->filter_coeffs) {
        free(resampler->filter_coeffs);
        resampler->filter_coeffs = NULL;
    }

    if (resampler->history_buffer) {
        free(resampler->history_buffer);
        resampler->history_buffer = NULL;
    }
}

// Utility functions
uint32_t resample_estimate_output_size(uint32_t input_samples, double ratio) {
    return (uint32_t)(input_samples * ratio + 1.0); // Add 1 for rounding
}

double resample_get_ratio(uint32_t input_rate, uint32_t output_rate) {
    if (input_rate == 0) return 1.0;
    return (double)output_rate / (double)input_rate;
}

// Predefined configurations
bool resample_init_audio_48k(resample_t *resampler, uint32_t input_rate) {
    return resample_init(resampler, input_rate, 48000);
}

bool resample_init_audio_44k(resample_t *resampler, uint32_t input_rate) {
    return resample_init(resampler, input_rate, 44100);
}
