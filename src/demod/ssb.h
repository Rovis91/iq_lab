/*
 * IQ Lab - SSB Demodulator Header
 *
 * Purpose: SSB (Single Sideband) demodulation with BFO mixing
  *
 *
 * This module provides professional SSB demodulation capabilities including:
 * - BFO (Beat Frequency Oscillator) mixing for baseband conversion
 * - USB (Upper Sideband) and LSB (Lower Sideband) mode support
 * - Configurable BFO frequency and low-pass filtering
 * - Complex mixing with quadrature oscillators
 * - Support for amateur radio and professional SSB communications
 *
 * The implementation uses efficient complex mixing and filtering
 * suitable for real-time SSB signal processing.
 *
 * Usage:
 *   ssb_demod_t ssb;
 *   ssb_demod_init_custom(&ssb, sample_rate, SSB_MODE_USB, bfo_freq, lpf_cutoff);
 *   float audio = ssb_demod_process_sample(&ssb, i_sample, q_sample);
 *
 * Algorithm:
 *   1. Generate quadrature BFO signals: cos(θ), sin(θ)
 *   2. Complex mixing: (I+jQ) * (cos(θ)+j*sin(θ))
 *   3. Low-pass filtering to select desired sideband
 *   4. Extract real part as audio output
 *
 * For USB: BFO frequency set above carrier (typically +1.5 to +3 kHz)
 * For LSB: BFO frequency set below carrier (typically -1.5 to -3 kHz)
 *
 * Dependencies: math.h, stdbool.h, stdint.h
 * Thread Safety: Not thread-safe (single demodulator instance per thread)
 * Performance: O(1) per sample, suitable for real-time processing
 */

#ifndef IQ_LAB_SSB_H
#define IQ_LAB_SSB_H

#include <stdint.h>
#include <stdbool.h>

// SSB demodulator modes
typedef enum {
    SSB_MODE_USB,    // Upper Sideband
    SSB_MODE_LSB     // Lower Sideband
} ssb_mode_t;

// SSB demodulator context
typedef struct {
    // Configuration
    float sample_rate;          // Input sample rate (Hz)
    ssb_mode_t mode;           // USB or LSB mode
    float bfo_frequency;       // Beat Frequency Oscillator frequency (Hz)

    // BFO state (for frequency shifting)
    float bfo_phase;           // Current BFO phase
    float bfo_phase_inc;       // BFO phase increment per sample

    // Low-pass filter for sideband rejection (simple IIR)
    float lpf_alpha;           // Low-pass filter coefficient
    float lpf_prev_i;          // Previous I filter output
    float lpf_prev_q;          // Previous Q filter output

    // Statistics
    uint32_t samples_processed; // Total samples processed
} ssb_demod_t;

// Initialize SSB demodulator with default parameters
bool ssb_demod_init(ssb_demod_t *ssb, float sample_rate);

// Initialize SSB demodulator with custom parameters
bool ssb_demod_init_custom(ssb_demod_t *ssb, float sample_rate, ssb_mode_t mode,
                          float bfo_frequency, float lpf_cutoff);

// Process a single IQ sample and return demodulated audio
float ssb_demod_process_sample(ssb_demod_t *ssb, float i_sample, float q_sample);

// Process a buffer of IQ samples
bool ssb_demod_process_buffer(ssb_demod_t *ssb,
                            const float *i_buffer, const float *q_buffer,
                            uint32_t num_samples, float *output_buffer);

// Reset SSB demodulator state
void ssb_demod_reset(ssb_demod_t *ssb);

// Set SSB mode (USB/LSB)
bool ssb_demod_set_mode(ssb_demod_t *ssb, ssb_mode_t mode);

// Set BFO frequency
bool ssb_demod_set_bfo_frequency(ssb_demod_t *ssb, float bfo_frequency);

// Get current SSB mode
ssb_mode_t ssb_demod_get_mode(const ssb_demod_t *ssb);

// Get current BFO frequency
float ssb_demod_get_bfo_frequency(const ssb_demod_t *ssb);

// Utility functions
float ssb_compute_lpf_coeff(float cutoff_freq, float sample_rate);
float ssb_compute_bfo_phase_inc(float bfo_freq, float sample_rate);

#endif // IQ_LAB_SSB_H
