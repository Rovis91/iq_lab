/*
 * IQ Lab - AM Demodulator Header
 *
 * Purpose: AM (Amplitude Modulation) demodulation with envelope detection
 * Author: IQ Lab Development Team
 *
 * This module provides professional AM demodulation capabilities including:
 * - Envelope detection using magnitude calculation: sqrt(I² + Q²)
 * - DC blocking filter to remove carrier DC component
 * - Configurable filter parameters for different AM standards
 * - Real-time statistics tracking (envelope range)
 * - Support for broadcast AM, amateur radio, and utility signals
 *
 * The implementation uses efficient envelope detection suitable for
 * real-time processing of AM signals with proper DC removal.
 *
 * Usage:
 *   am_demod_t am;
 *   am_demod_init_custom(&am, sample_rate, dc_cutoff);
 *   float audio = am_demod_process_sample(&am, i_sample, q_sample);
 *
 * Algorithm:
 *   1. Compute magnitude: envelope = sqrt(I² + Q²)
 *   2. Apply DC blocking: high-pass filter removes carrier DC
 *   3. Track statistics: envelope min/max for signal analysis
 *
 * Dependencies: math.h, stdbool.h, stdint.h
 * Thread Safety: Not thread-safe (single demodulator instance per thread)
 * Performance: O(1) per sample, suitable for real-time processing
 */

#ifndef IQ_LAB_AM_H
#define IQ_LAB_AM_H

#include <stdint.h>
#include <stdbool.h>

// AM demodulator context
typedef struct {
    // Configuration
    float sample_rate;          // Input sample rate (Hz)

    // DC blocking filter (high-pass)
    float dc_block_alpha;       // DC blocking filter coefficient
    float dc_block_prev;        // DC blocking filter previous output

    // Statistics
    uint32_t samples_processed; // Total samples processed
    float envelope_max;         // Maximum envelope value seen
    float envelope_min;         // Minimum envelope value seen
} am_demod_t;

// Initialize AM demodulator with default parameters
bool am_demod_init(am_demod_t *am, float sample_rate);

// Initialize AM demodulator with custom parameters
bool am_demod_init_custom(am_demod_t *am, float sample_rate, float dc_block_cutoff);

// Process a single IQ sample and return demodulated audio
float am_demod_process_sample(am_demod_t *am, float i_sample, float q_sample);

// Process a buffer of IQ samples
bool am_demod_process_buffer(am_demod_t *am,
                           const float *i_buffer, const float *q_buffer,
                           uint32_t num_samples, float *output_buffer);

// Reset AM demodulator state
void am_demod_reset(am_demod_t *am);

// Get statistics
float am_demod_get_envelope_max(const am_demod_t *am);
float am_demod_get_envelope_min(const am_demod_t *am);

// Utility functions
float am_compute_envelope(float i, float q);
float am_compute_dc_block_coeff(float cutoff_freq, float sample_rate);

#endif // IQ_LAB_AM_H
