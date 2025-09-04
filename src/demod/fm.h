/*
 * IQ Lab - FM Demodulator Header
 *
 * Purpose: FM (Frequency Modulation) demodulation with stereo support
  *
 *
 * This module provides professional FM demodulation capabilities including:
 * - FM discriminator using phase differentiation
 * - Stereo pilot tone detection (19kHz)
 * - Stereo subcarrier demodulation (38kHz)
 * - Matrix decoding for L/R channel separation
 * - Deemphasis filtering (50μs/75μs)
 * - DC blocking and audio processing
 *
 * The implementation supports both mono and stereo FM broadcast standards
 * with configurable parameters for different regional requirements.
 *
 * Usage:
 *   fm_demod_t fm;
 *   fm_demod_init_stereo(&fm, sample_rate, deviation, deemphasis, blend);
 *   fm_demod_process_stereo(&fm, i, q, &left, &right);
 *
 * Dependencies: math.h, stdbool.h, stdint.h
 * Thread Safety: Not thread-safe (single demodulator instance per thread)
 */

#ifndef IQ_LAB_FM_H
#define IQ_LAB_FM_H

#include <stdint.h>
#include <stdbool.h>
#include <complex.h>

// FM demodulator context
typedef struct {
    // Configuration
    float sample_rate;          // Input sample rate (Hz)
    float max_deviation;        // Maximum frequency deviation (Hz)
    float deemphasis_time;      // Deemphasis time constant (seconds)
    bool stereo_detection;      // Enable stereo pilot detection
    float stereo_blend;         // Stereo blend control (0.0 = mono, 1.0 = stereo)

    // Internal state
    float prev_phase;           // Previous phase for differentiation
    bool phase_initialized;     // Whether prev_phase has been set

    // Filters
    float dc_block_alpha;       // DC blocking filter coefficient
    float dc_block_prev;        // DC blocking filter previous output

    // Deemphasis filter (first-order IIR)
    float deemph_alpha;         // Deemphasis filter coefficient
    float deemph_prev;          // Deemphasis filter previous output

    // Stereo pilot detection (19kHz)
    float pilot_filter[16];     // Simple pilot tone filter taps
    float pilot_history[16];    // Pilot filter history
    uint32_t pilot_index;       // Current position in pilot filter
    float pilot_level;          // Detected pilot level

    // Stereo subcarrier demodulation (38kHz)
    float subcarrier_phase;     // 38kHz subcarrier phase
    float subcarrier_phase_inc; // 38kHz phase increment per sample
    float subcarrier_prev_i;    // Previous I for differentiation
    float subcarrier_prev_q;    // Previous Q for differentiation
    float subcarrier_lpf_prev;  // Low-pass filter for subcarrier envelope

    // Stereo matrix decoding
    float stereo_buffer[256];   // Buffer for stereo subcarrier samples
    uint32_t stereo_index;      // Current position in stereo buffer
    float stereo_sum;           // Running sum for L-R correlation

    // Output channels
    bool stereo_mode;           // Whether stereo signal is detected

    // Statistics
    uint32_t samples_processed; // Total samples processed
} fm_demod_t;

// Initialize FM demodulator with default parameters
bool fm_demod_init(fm_demod_t *fm, float sample_rate);

// Initialize FM demodulator with custom parameters
bool fm_demod_init_custom(fm_demod_t *fm, float sample_rate, float max_deviation,
                         float deemphasis_time, bool stereo_detection);

// Initialize FM demodulator with stereo blend control
bool fm_demod_init_stereo(fm_demod_t *fm, float sample_rate, float max_deviation,
                         float deemphasis_time, float stereo_blend);

// Process a single IQ sample and return demodulated mono audio
float fm_demod_process_sample(fm_demod_t *fm, float i_sample, float q_sample);

// Process IQ samples and return stereo audio (L+R in output[0], L-R in output[1])
bool fm_demod_process_stereo(fm_demod_t *fm, float i_sample, float q_sample,
                            float *left_output, float *right_output);

// Process a buffer of IQ samples (mono output)
bool fm_demod_process_buffer(fm_demod_t *fm,
                           const float *i_buffer, const float *q_buffer,
                           uint32_t num_samples, float *output_buffer);

// Process a buffer of IQ samples (stereo output)
bool fm_demod_process_stereo_buffer(fm_demod_t *fm,
                                  const float *i_buffer, const float *q_buffer,
                                  uint32_t num_samples,
                                  float *left_buffer, float *right_buffer);

// Reset FM demodulator state
void fm_demod_reset(fm_demod_t *fm);

// Get current pilot level (if stereo detection enabled)
float fm_demod_get_pilot_level(const fm_demod_t *fm);

// Check if stereo signal is detected
bool fm_demod_is_stereo(const fm_demod_t *fm);

// Set stereo blend level (0.0 = mono, 1.0 = stereo)
void fm_demod_set_stereo_blend(fm_demod_t *fm, float blend);

// Utility functions
float fm_phase_difference(float i1, float q1, float i2, float q2);
float fm_compute_deemphasis_coeff(float time_constant, float sample_rate);

#endif // IQ_LAB_FM_H
