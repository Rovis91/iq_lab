/*
 * IQ Lab - SSB Demodulator Implementation
 *
 * This file implements SSB (Single Sideband) demodulation using BFO mixing.
 * SSB demodulation is more complex than AM/FM but provides superior efficiency.
 *
 * SSB Demodulation Algorithm:
 * 1. BFO Generation: Create quadrature oscillator at desired frequency
 * 2. Complex Mixing: Mix input signal with BFO using complex multiplication
 *    (I+jQ) * (cos(θ) + j*sin(θ)) = (I*cos-Q*sin) + j(I*sin+Q*cos)
 * 3. Low-pass Filtering: Remove unwanted sideband and mixing products
 * 4. Audio Extraction: Take real part of filtered signal
 *
 * BFO Frequency Selection:
 * - USB (Upper Sideband): BFO above carrier (+1.5 to +3 kHz typical)
 * - LSB (Lower Sideband): BFO below carrier (-1.5 to -3 kHz typical)
 *
 * The choice of USB vs LSB depends on:
 * - Regional amateur radio band plans
 * - Signal bandwidth requirements
 * - Interference avoidance
 *
 * Performance Notes:
 * - O(1) complexity per sample
 * - Real-time capable up to high sample rates
 * - Fixed memory usage, no dynamic allocation
 * - Phase-continuous BFO prevents clicks/pops
 *
 * Applications:
 * - Amateur radio SSB communications (HF bands)
 * - Professional SSB communications
 * - Military and government SSB systems
 * - Any single-sideband modulated signal
 */

#include "ssb.h"
#include <math.h>
#include <string.h>
#include <assert.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Initialize SSB demodulator with default parameters
bool ssb_demod_init(ssb_demod_t *ssb, float sample_rate) {
    // Default to USB mode with 1.5 kHz BFO and 3 kHz LPF cutoff
    return ssb_demod_init_custom(ssb, sample_rate, SSB_MODE_USB, 1500.0f, 3000.0f);
}

// Initialize SSB demodulator with custom parameters
bool ssb_demod_init_custom(ssb_demod_t *ssb, float sample_rate, ssb_mode_t mode,
                          float bfo_frequency, float lpf_cutoff) {
    if (!ssb || sample_rate <= 0.0f || bfo_frequency <= 0.0f || lpf_cutoff <= 0.0f) {
        return false;
    }

    // Set configuration
    ssb->sample_rate = sample_rate;
    ssb->mode = mode;
    ssb->bfo_frequency = bfo_frequency;

    // Initialize BFO
    ssb->bfo_phase = 0.0f;
    ssb->bfo_phase_inc = ssb_compute_bfo_phase_inc(bfo_frequency, sample_rate);

    // For LSB mode, we need to negate the BFO frequency (mix with -f_bfo)
    if (mode == SSB_MODE_LSB) {
        ssb->bfo_phase_inc = -ssb->bfo_phase_inc;
    }

    // Initialize low-pass filter
    ssb->lpf_alpha = ssb_compute_lpf_coeff(lpf_cutoff, sample_rate);
    ssb->lpf_prev_i = 0.0f;
    ssb->lpf_prev_q = 0.0f;

    // Initialize statistics
    ssb->samples_processed = 0;

    return true;
}

// Process a single IQ sample and return demodulated audio
float ssb_demod_process_sample(ssb_demod_t *ssb, float i_sample, float q_sample) {
    if (!ssb) {
        return 0.0f;
    }

    // Generate BFO signal
    float bfo_cos = cosf(ssb->bfo_phase);
    float bfo_sin = sinf(ssb->bfo_phase);

    // Mix input with BFO (complex multiplication)
    // (I + jQ) * (cos(θ) + j*sin(θ)) = (I*cos - Q*sin) + j(I*sin + Q*cos)
    float mixed_i = i_sample * bfo_cos - q_sample * bfo_sin;
    float mixed_q = i_sample * bfo_sin + q_sample * bfo_cos;

    // Apply low-pass filter to remove unwanted sideband
    float filtered_i = ssb->lpf_alpha * (mixed_i - ssb->lpf_prev_i) + ssb->lpf_prev_i;
    float filtered_q = ssb->lpf_alpha * (mixed_q - ssb->lpf_prev_q) + ssb->lpf_prev_q;

    // Update filter state
    ssb->lpf_prev_i = filtered_i;
    ssb->lpf_prev_q = filtered_q;

    // Update BFO phase
    ssb->bfo_phase += ssb->bfo_phase_inc;
    if (ssb->bfo_phase > 2.0f * M_PI) {
        ssb->bfo_phase -= 2.0f * M_PI;
    } else if (ssb->bfo_phase < -2.0f * M_PI) {
        ssb->bfo_phase += 2.0f * M_PI;
    }

    ssb->samples_processed++;

    // Return the real part as audio (imaginary part contains the other sideband)
    return filtered_i;
}

// Process a buffer of IQ samples
bool ssb_demod_process_buffer(ssb_demod_t *ssb,
                            const float *i_buffer, const float *q_buffer,
                            uint32_t num_samples, float *output_buffer) {
    if (!ssb || !i_buffer || !q_buffer || !output_buffer || num_samples == 0) {
        return false;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        output_buffer[i] = ssb_demod_process_sample(ssb, i_buffer[i], q_buffer[i]);
    }

    return true;
}

// Reset SSB demodulator state
void ssb_demod_reset(ssb_demod_t *ssb) {
    if (!ssb) return;

    ssb->bfo_phase = 0.0f;
    ssb->lpf_prev_i = 0.0f;
    ssb->lpf_prev_q = 0.0f;
    ssb->samples_processed = 0;
}

// Set SSB mode (USB/LSB)
bool ssb_demod_set_mode(ssb_demod_t *ssb, ssb_mode_t mode) {
    if (!ssb) return false;

    ssb->mode = mode;

    // Recalculate BFO phase increment based on new mode
    ssb->bfo_phase_inc = ssb_compute_bfo_phase_inc(ssb->bfo_frequency, ssb->sample_rate);

    if (mode == SSB_MODE_LSB) {
        ssb->bfo_phase_inc = -ssb->bfo_phase_inc;
    }

    return true;
}

// Set BFO frequency
bool ssb_demod_set_bfo_frequency(ssb_demod_t *ssb, float bfo_frequency) {
    if (!ssb || bfo_frequency <= 0.0f) return false;

    ssb->bfo_frequency = bfo_frequency;

    // Recalculate BFO phase increment
    ssb->bfo_phase_inc = ssb_compute_bfo_phase_inc(bfo_frequency, ssb->sample_rate);

    if (ssb->mode == SSB_MODE_LSB) {
        ssb->bfo_phase_inc = -ssb->bfo_phase_inc;
    }

    return true;
}

// Get current SSB mode
ssb_mode_t ssb_demod_get_mode(const ssb_demod_t *ssb) {
    return ssb ? ssb->mode : SSB_MODE_USB;
}

// Get current BFO frequency
float ssb_demod_get_bfo_frequency(const ssb_demod_t *ssb) {
    return ssb ? ssb->bfo_frequency : 0.0f;
}

// Compute low-pass filter coefficient
float ssb_compute_lpf_coeff(float cutoff_freq, float sample_rate) {
    // First-order low-pass filter coefficient
    // alpha = cutoff_freq / (cutoff_freq + sample_rate / (2π))
    // This gives -3dB at cutoff_freq
    return cutoff_freq / (cutoff_freq + sample_rate / (2.0f * M_PI));
}

// Compute BFO phase increment per sample
float ssb_compute_bfo_phase_inc(float bfo_freq, float sample_rate) {
    // Phase increment = 2π * f_bfo / f_sample
    return 2.0f * M_PI * bfo_freq / sample_rate;
}
