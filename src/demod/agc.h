#ifndef IQ_LAB_AGC_H
#define IQ_LAB_AGC_H

#include <stdint.h>
#include <stdbool.h>

// AGC (Automatic Gain Control) context
typedef struct {
    // Configuration parameters
    float attack_time;      // Attack time constant (seconds)
    float release_time;     // Release time constant (seconds)
    float reference_level;  // Target output level (0.0 to 1.0)
    float max_gain;         // Maximum gain limit
    float hang_time;        // Hang time before release (seconds)

    // Internal state
    float current_gain;     // Current gain value
    float peak_detector;    // Current peak detector value
    float hang_counter;     // Hang time counter
    uint32_t sample_rate;   // Sample rate for time calculations

    // Pre-computed coefficients
    float attack_coeff;     // Attack coefficient (1 - exp(-1/(attack_time * sample_rate)))
    float release_coeff;    // Release coefficient (1 - exp(-1/(release_time * sample_rate)))
} agc_t;

// Initialize AGC with default parameters
bool agc_init(agc_t *agc, uint32_t sample_rate);

// Initialize AGC with custom parameters
bool agc_init_custom(agc_t *agc, uint32_t sample_rate,
                    float attack_time, float release_time,
                    float reference_level, float max_gain, float hang_time);

// Process a single audio sample through AGC
float agc_process_sample(agc_t *agc, float input_sample);

// Process a buffer of audio samples through AGC
bool agc_process_buffer(agc_t *agc, float *buffer, uint32_t num_samples);

// Reset AGC state
void agc_reset(agc_t *agc);

// Get current gain value
float agc_get_current_gain(const agc_t *agc);

// Set reference level
void agc_set_reference_level(agc_t *agc, float reference_level);

// Utility functions for common AGC configurations
bool agc_init_fast_attack(agc_t *agc, uint32_t sample_rate);     // Fast attack for speech/music
bool agc_init_slow_attack(agc_t *agc, uint32_t sample_rate);     // Slow attack for broadcast
bool agc_init_compressor(agc_t *agc, uint32_t sample_rate);      // Compressor-like behavior

#endif // IQ_LAB_AGC_H
