/*
 * IQ Lab - pfb.c: Polyphase Filter Bank Channelizer Implementation
 *
 * Purpose: Efficient channelization of wideband IQ signals using polyphase
 * filter bank techniques for high isolation and minimal aliasing.
 *
 * Algorithm Overview:
 * 1. Design prototype low-pass filter (Kaiser window)
 * 2. Decompose into polyphase branches
 * 3. Process input through filter branches
 * 4. FFT to frequency domain
 * 5. Extract channel sub-bands
 *
 *
 * Date: 2025
 */

#include "pfb.h"
#include "../iq_core/fft.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Forward declarations
static bool process_fft_block(pfb_t *pfb);

/**
 * @brief Initialize PFB configuration with sensible defaults
 */
bool pfb_config_init(pfb_config_t *config, uint32_t num_channels,
                    double sample_rate, double channel_bandwidth) {
    if (!config || num_channels < 4 || num_channels > 32 ||
        sample_rate <= 0.0 || channel_bandwidth <= 0.0) {
        return false;
    }

    config->num_channels = num_channels;
    config->sample_rate = sample_rate;
    config->channel_bandwidth = channel_bandwidth;

    // Calculate optimal filter parameters
    config->filter_length = 4096;  // Default filter length (power of 2)
    config->fft_size = config->filter_length / num_channels;  // Ensure divisibility
    config->overlap_factor = 0.1;  // 10% overlap
    config->kaiser_beta = 8.6;     // Good stopband attenuation
    config->block_size = config->fft_size;  // Process one FFT block at a time

    return true;
}

/**
 * @brief Validate PFB configuration parameters
 */
bool pfb_config_validate(const pfb_config_t *config) {
    if (!config) return false;

    // Basic parameter validation
    if (config->num_channels < 4 || config->num_channels > 32) return false;
    if (config->sample_rate <= 0.0) return false;
    if (config->channel_bandwidth <= 0.0) return false;
    if (config->filter_length == 0 || (config->filter_length & (config->filter_length - 1)) != 0) return false;
    if (config->fft_size == 0 || config->fft_size > config->filter_length) return false;
    if (config->overlap_factor < 0.0 || config->overlap_factor >= 1.0) return false;
    if (config->kaiser_beta < 0.0) return false;
    if (config->block_size == 0) return false;

    // Check that filter length is compatible with number of channels
    if (config->filter_length % config->num_channels != 0) return false;

    return true;
}

/**
 * @brief Design Kaiser window
 */
static void design_kaiser_window(float *window, uint32_t length, double beta) {
    double sum = 0.0;

    // Calculate window values
    for (uint32_t i = 0; i < length; i++) {
        double x = 2.0 * i / (length - 1.0) - 1.0;  // Map to [-1, 1]
        double t = 1.0 - x * x;

        if (fabs(x) > 1.0) {
            window[i] = 0.0f;
        } else {
            // Modified Bessel function of first kind, order 0
            double bessel_i0 = 1.0;
            double term = 1.0;
            for (int k = 1; k <= 10; k++) {
                term *= (beta * beta / 4.0) / (k * k);
                bessel_i0 += term;
                if (term < 1e-12) break;
            }

            window[i] = (float)(bessel_i0 / bessel_i0 * sqrt(t));
        }
        sum += window[i];
    }

    // Normalize
    if (sum > 0.0) {
        for (uint32_t i = 0; i < length; i++) {
            window[i] /= (float)sum;
        }
    }
}

/**
 * @brief Design prototype low-pass filter
 */
static bool design_prototype_filter(pfb_t *pfb) {
    uint32_t length = pfb->config.filter_length;
    double sample_rate = pfb->config.sample_rate;
    double cutoff_freq = pfb->config.channel_bandwidth / 2.0;
    double beta = pfb->config.kaiser_beta;

    // Allocate filter memory
    pfb->prototype_filter = (float *)malloc(length * sizeof(float));
    if (!pfb->prototype_filter) return false;

    // Design Kaiser window
    float *window = (float *)malloc(length * sizeof(float));
    if (!window) {
        free(pfb->prototype_filter);
        return false;
    }

    design_kaiser_window(window, length, beta);

    // Design sinc filter and apply window
    double wc = 2.0 * M_PI * cutoff_freq / sample_rate;
    double center = (length - 1) / 2.0;

    for (uint32_t i = 0; i < length; i++) {
        double x = i - center;
        double sinc_val;

        if (fabs(x) < 1e-10) {
            sinc_val = 1.0;
        } else {
            sinc_val = sin(wc * x) / (M_PI * x);
        }

        pfb->prototype_filter[i] = (float)(sinc_val * window[i]);
    }

    free(window);
    return true;
}

/**
 * @brief Decompose prototype filter into polyphase branches
 */
static bool decompose_polyphase(pfb_t *pfb) {
    uint32_t num_branches = pfb->config.num_channels;
    uint32_t filter_length = pfb->config.filter_length;

    pfb->num_branches = num_branches;
    pfb->branch_length = filter_length / num_branches;

    // Allocate polyphase filter branches
    pfb->polyphase_filters = (float **)malloc(num_branches * sizeof(float *));
    if (!pfb->polyphase_filters) return false;

    for (uint32_t branch = 0; branch < num_branches; branch++) {
        pfb->polyphase_filters[branch] = (float *)malloc(pfb->branch_length * sizeof(float));
        if (!pfb->polyphase_filters[branch]) {
            // Cleanup on error
            for (uint32_t i = 0; i < branch; i++) {
                free(pfb->polyphase_filters[i]);
            }
            free(pfb->polyphase_filters);
            return false;
        }

        // Extract polyphase branch (reverse order for convolution)
        for (uint32_t i = 0; i < pfb->branch_length; i++) {
            uint32_t idx = branch + i * num_branches;
            if (idx < filter_length) {
                pfb->polyphase_filters[branch][pfb->branch_length - 1 - i] =
                    pfb->prototype_filter[idx];
            } else {
                pfb->polyphase_filters[branch][pfb->branch_length - 1 - i] = 0.0f;
            }
        }
    }

    return true;
}

/**
 * @brief Initialize channel structures
 */
static bool initialize_channels(pfb_t *pfb) {
    uint32_t num_channels = pfb->config.num_channels;

    pfb->channels = (pfb_channel_t *)malloc(num_channels * sizeof(pfb_channel_t));
    if (!pfb->channels) return false;

    pfb->num_channels = num_channels;

    for (uint32_t i = 0; i < num_channels; i++) {
        pfb_channel_t *chan = &pfb->channels[i];

        chan->buffer_size = pfb->config.block_size;
        chan->buffer = (float complex *)malloc(chan->buffer_size * sizeof(float complex));
        if (!chan->buffer) {
            // Cleanup on error
            for (uint32_t j = 0; j < i; j++) {
                free(pfb->channels[j].buffer);
            }
            free(pfb->channels);
            return false;
        }

        chan->samples_written = 0;
        chan->channel_index = i;
        chan->bandwidth = pfb->config.channel_bandwidth;

        // Calculate center frequency for this channel
        double channel_spacing = pfb->config.sample_rate / num_channels;
        chan->center_frequency = (i - (int)(num_channels / 2)) * channel_spacing;
    }

    return true;
}

/**
 * @brief Create PFB channelizer instance
 */
pfb_t *pfb_create(const pfb_config_t *config) {
    if (!pfb_config_validate(config)) {
        return NULL;
    }

    pfb_t *pfb = (pfb_t *)malloc(sizeof(pfb_t));
    if (!pfb) return NULL;

    // Copy configuration
    memcpy(&pfb->config, config, sizeof(pfb_config_t));

    // Initialize state
    pfb->initialized = false;
    pfb->input_index = 0;

    // Allocate input buffer
    pfb->input_buffer_size = config->block_size * config->num_channels;
    pfb->input_buffer = (fft_complex_t *)malloc(pfb->input_buffer_size * sizeof(fft_complex_t));
    if (!pfb->input_buffer) {
        free(pfb);
        return NULL;
    }

    // Design prototype filter
    if (!design_prototype_filter(pfb)) {
        free(pfb->input_buffer);
        free(pfb);
        return NULL;
    }

    // Decompose into polyphase branches
    if (!decompose_polyphase(pfb)) {
        free(pfb->prototype_filter);
        free(pfb->input_buffer);
        free(pfb);
        return NULL;
    }

    // Initialize FFT
    pfb->fft_plan = fft_plan_create(config->fft_size, FFT_FORWARD);
    if (!pfb->fft_plan) {
        // Cleanup
        for (uint32_t i = 0; i < pfb->num_branches; i++) {
            free(pfb->polyphase_filters[i]);
        }
        free(pfb->polyphase_filters);
        free(pfb->prototype_filter);
        free(pfb->input_buffer);
        free(pfb);
        return NULL;
    }

    // Allocate FFT buffers
    pfb->fft_input = (fft_complex_t *)malloc(config->fft_size * sizeof(fft_complex_t));
    pfb->fft_output = (fft_complex_t *)malloc(config->fft_size * sizeof(fft_complex_t));
    if (!pfb->fft_input || !pfb->fft_output) {
        fft_plan_destroy(pfb->fft_plan);
        for (uint32_t i = 0; i < pfb->num_branches; i++) {
            free(pfb->polyphase_filters[i]);
        }
        free(pfb->polyphase_filters);
        free(pfb->prototype_filter);
        free(pfb->input_buffer);
        free(pfb);
        return NULL;
    }

    // Initialize channels
    if (!initialize_channels(pfb)) {
        free(pfb->fft_output);
        free(pfb->fft_input);
        fft_plan_destroy(pfb->fft_plan);
        for (uint32_t i = 0; i < pfb->num_branches; i++) {
            free(pfb->polyphase_filters[i]);
        }
        free(pfb->polyphase_filters);
        free(pfb->prototype_filter);
        free(pfb->input_buffer);
        free(pfb);
        return NULL;
    }

    pfb->initialized = true;
    return pfb;
}

/**
 * @brief Destroy PFB channelizer instance
 */
void pfb_destroy(pfb_t *pfb) {
    if (!pfb) return;

    // Free channels
    if (pfb->channels) {
        for (uint32_t i = 0; i < pfb->num_channels; i++) {
            free(pfb->channels[i].buffer);
        }
        free(pfb->channels);
    }

    // Free FFT resources
    if (pfb->fft_output) free(pfb->fft_output);
    if (pfb->fft_input) free(pfb->fft_input);
    if (pfb->fft_plan) fft_plan_destroy(pfb->fft_plan);

    // Free polyphase filters
    if (pfb->polyphase_filters) {
        for (uint32_t i = 0; i < pfb->num_branches; i++) {
            free(pfb->polyphase_filters[i]);
        }
        free(pfb->polyphase_filters);
    }

    // Free prototype filter and input buffer
    free(pfb->prototype_filter);
    free(pfb->input_buffer);

    free(pfb);
}

/**
 * @brief Process a block of IQ samples through the PFB
 */
int32_t pfb_process_block(pfb_t *pfb, const float complex *input, uint32_t input_length) {
    if (!pfb || !pfb->initialized || !input || input_length == 0) {
        return -1;
    }

    uint32_t samples_processed = 0;

    while (samples_processed < input_length) {
        // Fill input buffer
        uint32_t samples_to_copy = pfb->input_buffer_size - pfb->input_index;
        if (samples_to_copy > input_length - samples_processed) {
            samples_to_copy = input_length - samples_processed;
        }

        memcpy(&pfb->input_buffer[pfb->input_index],
               &input[samples_processed],
               samples_to_copy * sizeof(float complex));

        pfb->input_index += samples_to_copy;
        samples_processed += samples_to_copy;

        // Process complete blocks
        while (pfb->input_index >= pfb->config.block_size) {
            if (!process_fft_block(pfb)) {
                return -1;
            }
            pfb->input_index -= pfb->config.block_size;
        }
    }

    return samples_processed;
}

/**
 * @brief Process one FFT block through the PFB
 */
static bool process_fft_block(pfb_t *pfb) {
    uint32_t fft_size = pfb->config.fft_size;
    uint32_t num_channels = pfb->config.num_channels;

    // Prepare FFT input (polyphase filtering)
    for (uint32_t i = 0; i < fft_size; i++) {
        pfb->fft_input[i] = 0.0 + 0.0 * I;

        // Sum contributions from all polyphase branches
        for (uint32_t branch = 0; branch < num_channels; branch++) {
            uint32_t input_idx = pfb->input_index - fft_size + i;
            if (input_idx < pfb->input_buffer_size) {
                uint32_t filter_idx = i % pfb->branch_length;
                fft_complex_t input_sample = pfb->input_buffer[input_idx];
                double filter_coeff = (double)pfb->polyphase_filters[branch][filter_idx];
                pfb->fft_input[i] += input_sample * filter_coeff;
            }
        }
    }

    // Execute FFT
    if (!fft_execute(pfb->fft_plan, pfb->fft_input, pfb->fft_output)) {
        return false;
    }

    // Extract channels from FFT output
    for (uint32_t chan = 0; chan < num_channels; chan++) {
        pfb_channel_t *channel = &pfb->channels[chan];

        // Ensure buffer has space
        if (channel->samples_written >= channel->buffer_size) {
            // Buffer full - in a real implementation, you'd want to handle this
            continue;
        }

        // Extract channel sample (with proper frequency mapping)
        uint32_t fft_idx = (chan * fft_size / num_channels) % fft_size;
        // Convert from double complex to float complex for channel storage
        channel->buffer[channel->samples_written] = (float complex)pfb->fft_output[fft_idx];
        channel->samples_written++;
    }

    return true;
}

/**
 * @brief Get channel output buffer
 */
const float complex *pfb_get_channel_output(const pfb_t *pfb, uint32_t channel_index,
                                          uint32_t *samples) {
    if (!pfb || !pfb->initialized || channel_index >= pfb->num_channels || !samples) {
        return NULL;
    }

    *samples = pfb->channels[channel_index].samples_written;
    return pfb->channels[channel_index].buffer;
}

/**
 * @brief Reset channel output buffer
 */
bool pfb_reset_channel_output(pfb_t *pfb, uint32_t channel_index) {
    if (!pfb || !pfb->initialized || channel_index >= pfb->num_channels) {
        return false;
    }

    pfb->channels[channel_index].samples_written = 0;
    return true;
}

/**
 * @brief Get PFB processing statistics
 */
bool pfb_get_statistics(const pfb_t *pfb, uint64_t *total_samples,
                       uint64_t *channel_samples, uint32_t channel_samples_size) {
    if (!pfb || !pfb->initialized || !total_samples) {
        return false;
    }

    // This is a simplified implementation - in practice you'd track actual sample counts
    *total_samples = 0;

    if (channel_samples && channel_samples_size >= pfb->num_channels) {
        for (uint32_t i = 0; i < pfb->num_channels; i++) {
            channel_samples[i] = pfb->channels[i].samples_written;
        }
    }

    return true;
}

/**
 * @brief Calculate expected channel isolation
 */
double pfb_calculate_isolation(const pfb_config_t *config) {
    if (!config) return -1.0;

    // Simplified calculation based on Kaiser window parameters
    // In practice, this would be more sophisticated
    double beta = config->kaiser_beta;
    double isolation_db = 10.0 * log10(beta / 0.1102 + 1.0) - 20.0 * log10(beta / 0.5842 + 0.07886);

    return isolation_db;
}

/**
 * @brief Get channel center frequency
 */
double pfb_get_channel_frequency(const pfb_t *pfb, uint32_t channel_index) {
    if (!pfb || !pfb->initialized || channel_index >= pfb->num_channels) {
        return -1.0;
    }

    return pfb->channels[channel_index].center_frequency;
}

/**
 * @brief Get channel bandwidth
 */
double pfb_get_channel_bandwidth(const pfb_t *pfb, uint32_t channel_index) {
    if (!pfb || !pfb->initialized || channel_index >= pfb->num_channels) {
        return -1.0;
    }

    return pfb->channels[channel_index].bandwidth;
}

/**
 * @brief Get string representation of PFB configuration
 */
bool pfb_get_config_string(const pfb_t *pfb, char *buffer, uint32_t buffer_size) {
    if (!pfb || !buffer || buffer_size == 0) {
        return false;
    }

    int written = snprintf(buffer, buffer_size,
                          "PFB Config: %u channels, %.0f Hz sample rate, %.0f Hz/channel, "
                          "%u tap filter, %u FFT size",
                          pfb->config.num_channels, pfb->config.sample_rate,
                          pfb->config.channel_bandwidth, pfb->config.filter_length,
                          pfb->config.fft_size);

    return written > 0 && (uint32_t)written < buffer_size;
}

/**
 * @brief Reset PFB processing state
 */
bool pfb_reset(pfb_t *pfb) {
    if (!pfb || !pfb->initialized) {
        return false;
    }

    pfb->input_index = 0;

    // Reset all channel buffers
    for (uint32_t i = 0; i < pfb->num_channels; i++) {
        pfb->channels[i].samples_written = 0;
    }

    return true;
}
