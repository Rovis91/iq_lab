/*
 * IQ Lab - AM Demodulator Implementation
 *
 * This file implements AM demodulation using envelope detection.
 * The algorithm is optimized for real-time processing with minimal latency.
 *
 * AM Demodulation Algorithm:
 * 1. Magnitude calculation: envelope = sqrt(I² + Q²)
 * 2. DC blocking: High-pass filter removes carrier DC component
 * 3. Statistics tracking: Monitor envelope range for signal analysis
 *
 * The DC blocking filter is crucial for AM demodulation because:
 * - AM signals have a strong DC component from the carrier
 * - This DC component would otherwise dominate the audio output
 * - The high-pass filter removes this while preserving modulation
 *
 * Performance Characteristics:
 * - O(1) complexity per sample
 * - Minimal memory usage (fixed state)
 * - Suitable for real-time processing
 * - No dynamic memory allocation during processing
 *
 * Applications:
 * - AM broadcast radio (MW, SW)
 * - Amateur radio AM communications
 * - Utility and navigation signals
 * - Any amplitude-modulated signal
 */

#include "am.h"
#include <math.h>
#include <string.h>
#include <assert.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Initialize AM demodulator with default parameters
bool am_demod_init(am_demod_t *am, float sample_rate) {
    // Default DC blocking cutoff at 100 Hz
    return am_demod_init_custom(am, sample_rate, 100.0f);
}

// Initialize AM demodulator with custom parameters
bool am_demod_init_custom(am_demod_t *am, float sample_rate, float dc_block_cutoff) {
    if (!am || sample_rate <= 0.0f || dc_block_cutoff <= 0.0f) {
        return false;
    }

    // Set configuration
    am->sample_rate = sample_rate;

    // Initialize DC blocking filter
    am->dc_block_alpha = am_compute_dc_block_coeff(dc_block_cutoff, sample_rate);
    am->dc_block_prev = 0.0f;

    // Initialize statistics
    am->samples_processed = 0;
    am->envelope_max = 0.0f;
    am->envelope_min = 1.0f;  // Initialize to high value

    return true;
}

// Process a single IQ sample and return demodulated audio
float am_demod_process_sample(am_demod_t *am, float i_sample, float q_sample) {
    if (!am) {
        return 0.0f;
    }

    // Compute envelope (magnitude)
    float envelope = am_compute_envelope(i_sample, q_sample);

    // Update statistics
    if (envelope > am->envelope_max) {
        am->envelope_max = envelope;
    }
    if (envelope < am->envelope_min) {
        am->envelope_min = envelope;
    }

    // Apply DC blocking filter (high-pass)
    float dc_blocked = envelope - am->dc_block_prev + am->dc_block_alpha * am->dc_block_prev;
    am->dc_block_prev = dc_blocked;

    am->samples_processed++;
    return dc_blocked;
}

// Process a buffer of IQ samples
bool am_demod_process_buffer(am_demod_t *am,
                           const float *i_buffer, const float *q_buffer,
                           uint32_t num_samples, float *output_buffer) {
    if (!am || !i_buffer || !q_buffer || !output_buffer || num_samples == 0) {
        return false;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        output_buffer[i] = am_demod_process_sample(am, i_buffer[i], q_buffer[i]);
    }

    return true;
}

// Reset AM demodulator state
void am_demod_reset(am_demod_t *am) {
    if (!am) return;

    am->dc_block_prev = 0.0f;
    am->samples_processed = 0;
    am->envelope_max = 0.0f;
    am->envelope_min = 1.0f;
}

// Get statistics
float am_demod_get_envelope_max(const am_demod_t *am) {
    return am ? am->envelope_max : 0.0f;
}

float am_demod_get_envelope_min(const am_demod_t *am) {
    return am ? am->envelope_min : 0.0f;
}

// Compute envelope (magnitude) of complex sample
float am_compute_envelope(float i, float q) {
    return sqrtf(i * i + q * q);
}

// Compute DC blocking filter coefficient
float am_compute_dc_block_coeff(float cutoff_freq, float sample_rate) {
    // First-order high-pass filter coefficient
    // alpha = cutoff_freq / (cutoff_freq + sample_rate / (2π))
    // This gives -3dB at cutoff_freq
    return cutoff_freq / (cutoff_freq + sample_rate / (2.0f * M_PI));
}
