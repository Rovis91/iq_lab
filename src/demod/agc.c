/*
 * IQ Lab - Automatic Gain Control (AGC) Module
 *
 * Purpose: Professional audio level control with peak-based gain adjustment
 * Author: IQ Lab Development Team
 *
 * This module provides sophisticated automatic gain control for audio signals,
 * essential for consistent playback levels in demodulated audio. It uses
 * peak-based detection with configurable attack/release characteristics.
 *
 * Key Features:
 * - Peak-based gain control with exponential smoothing
 * - Configurable attack/release time constants
 * - Adjustable reference level and maximum gain limits
 * - Hang time for stable gain in presence of short peaks
 * - Multiple preset configurations for different applications
 * - Real-time buffer processing capability
 *
 * Usage:
 *   agc_t agc;
 *   agc_init_custom(&agc, sample_rate, 0.01f, 0.1f, 0.8f, 20.0f, 0.5f);
 *   float output = agc_process_sample(&agc, input_sample);
 *
 * Algorithm:
 * 1. Peak Detection: Track maximum absolute value using exponential smoothing
 * 2. Gain Calculation: target_gain = reference_level / peak_detector
 * 3. Gain Limiting: Clamp gain between 1.0 and max_gain
 * 4. Gain Smoothing: Gradually adjust current_gain to target_gain
 * 5. Hang Time: Maintain gain during short signal drops
 *
 * Preset Configurations:
 * - Fast Attack: Quick response for speech/music
 * - Slow Attack: Smooth response for broadcast
 * - Compressor: Heavy compression for dynamic range control
 *
 * Technical Details:
 * - Attack Coefficient: 1 - exp(-1/(attack_time * sample_rate))
 * - Release Coefficient: 1 - exp(-1/(release_time * sample_rate))
 * - Peak Smoothing: Exponential moving average
 * - Gain Smoothing: Linear interpolation for artifact reduction
 *
 * Performance:
 * - O(1) complexity per sample
 * - Memory efficient (fixed state size)
 * - Real-time capable for all sample rates
 * - Minimal latency (single sample delay)
 *
 * Applications:
 * - AM/FM demodulation audio level stabilization
 * - Signal processing chain gain control
 * - Audio recording level normalization
 * - Communications receiver audio output
 *
 * Dependencies: math.h, string.h
 * Thread Safety: Not thread-safe (single AGC instance per thread)
 * Error Handling: Parameter validation with graceful degradation
 */

#include "agc.h"
#include <math.h>
#include <string.h>

// Initialize AGC with default parameters suitable for audio demodulation
bool agc_init(agc_t *agc, uint32_t sample_rate) {
    return agc_init_custom(agc, sample_rate, 0.01f, 0.1f, 0.8f, 20.0f, 0.5f);
}

// Initialize AGC with custom parameters
bool agc_init_custom(agc_t *agc, uint32_t sample_rate,
                    float attack_time, float release_time,
                    float reference_level, float max_gain, float hang_time) {
    if (!agc || sample_rate == 0) {
        return false;
    }

    if (attack_time <= 0.0f || release_time <= 0.0f ||
        reference_level <= 0.0f || max_gain <= 0.0f || hang_time < 0.0f) {
        return false;
    }

    // Set parameters
    agc->attack_time = attack_time;
    agc->release_time = release_time;
    agc->reference_level = reference_level;
    agc->max_gain = max_gain;
    agc->hang_time = hang_time;
    agc->sample_rate = sample_rate;

    // Initialize state
    agc->current_gain = 1.0f;
    agc->peak_detector = 0.0f;
    agc->hang_counter = 0.0f;

    // Pre-compute coefficients for exponential smoothing
    // attack_coeff = 1 - exp(-1/(attack_time * sample_rate))
    agc->attack_coeff = 1.0f - expf(-1.0f / (attack_time * (float)sample_rate));

    // release_coeff = 1 - exp(-1/(release_time * sample_rate))
    agc->release_coeff = 1.0f - expf(-1.0f / (release_time * (float)sample_rate));

    return true;
}

// Process a single audio sample through AGC
float agc_process_sample(agc_t *agc, float input_sample) {
    if (!agc) {
        return input_sample;
    }

    // Get absolute value for peak detection (always use raw input)
    float abs_input = fabsf(input_sample);

    // Update peak detector with exponential smoothing (based on raw input)
    if (abs_input > agc->peak_detector) {
        // Attack: fast rise
        agc->peak_detector = agc->attack_coeff * (abs_input - agc->peak_detector) + agc->peak_detector;
        agc->hang_counter = agc->hang_time * (float)agc->sample_rate; // Reset hang counter
    } else {
        // Release: slow decay (if hang time expired)
        if (agc->hang_counter <= 0.0f) {
            agc->peak_detector = agc->release_coeff * (abs_input - agc->peak_detector) + agc->peak_detector;
        } else {
            agc->hang_counter -= 1.0f;
        }
    }

    // Calculate required gain
    float target_gain = 1.0f;
    if (agc->peak_detector > 0.0f) {
        target_gain = agc->reference_level / agc->peak_detector;
    }

    // Limit maximum gain
    if (target_gain > agc->max_gain) {
        target_gain = agc->max_gain;
    }

    // Smooth gain changes to avoid artifacts
    // Faster smoothing for better response
    float gain_change = target_gain - agc->current_gain;
    agc->current_gain += 0.2f * gain_change; // Increased from 0.1 to 0.2

    // Ensure gain doesn't go below 1.0 (no attenuation)
    if (agc->current_gain < 1.0f) {
        agc->current_gain = 1.0f;
    }

    // Apply gain to input sample
    return input_sample * agc->current_gain;
}

// Process a buffer of audio samples through AGC
bool agc_process_buffer(agc_t *agc, float *buffer, uint32_t num_samples) {
    if (!agc || !buffer || num_samples == 0) {
        return false;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        buffer[i] = agc_process_sample(agc, buffer[i]);
    }

    return true;
}

// Reset AGC state
void agc_reset(agc_t *agc) {
    if (!agc) return;

    agc->current_gain = 1.0f;
    agc->peak_detector = 0.0f;
    agc->hang_counter = 0.0f;
}

// Get current gain value
float agc_get_current_gain(const agc_t *agc) {
    return agc ? agc->current_gain : 1.0f;
}

// Set reference level
void agc_set_reference_level(agc_t *agc, float reference_level) {
    if (agc && reference_level > 0.0f) {
        agc->reference_level = reference_level;
    }
}

// Fast attack AGC for speech/music (quick response to peaks)
bool agc_init_fast_attack(agc_t *agc, uint32_t sample_rate) {
    return agc_init_custom(agc, sample_rate, 0.001f, 0.2f, 0.7f, 10.0f, 0.1f);
}

// Slow attack AGC for broadcast (smooth response)
bool agc_init_slow_attack(agc_t *agc, uint32_t sample_rate) {
    return agc_init_custom(agc, sample_rate, 0.1f, 0.5f, 0.8f, 20.0f, 1.0f);
}

// Compressor-like AGC (heavy compression)
bool agc_init_compressor(agc_t *agc, uint32_t sample_rate) {
    return agc_init_custom(agc, sample_rate, 0.01f, 0.01f, 0.5f, 4.0f, 0.0f);
}
