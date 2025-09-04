/*
 * IQ Lab - pfb.h: Polyphase Filter Bank Channelizer
 *
 * Purpose: Provides efficient channelization of wideband IQ signals using
 * polyphase filter bank techniques for minimal aliasing and high isolation.
 *
  *
 * Date: 2025
 *
 * Key Features:
 * - Configurable number of channels (4-32)
 * - Automatic Kaiser window filter design
 * - High channel isolation (>55 dB typical)
 * - Memory-efficient block processing
 * - Real-time capable implementation
 */

#ifndef PFB_H
#define PFB_H

#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include "../iq_core/fft.h"

// Forward declarations
typedef struct pfb_t pfb_t;
typedef struct pfb_channel_t pfb_channel_t;
typedef struct pfb_config_t pfb_config_t;

/**
 * @brief PFB Configuration Structure
 *
 * Defines the parameters for polyphase filter bank operation
 */
struct pfb_config_t {
    uint32_t num_channels;        // Number of output channels (4-32)
    uint32_t filter_length;       // Length of prototype filter (power of 2)
    uint32_t fft_size;           // FFT size for polyphase decomposition
    double sample_rate;          // Input sample rate (Hz)
    double channel_bandwidth;    // Bandwidth per channel (Hz)
    double overlap_factor;       // Channel overlap (0.0-0.5)
    double kaiser_beta;          // Kaiser window beta parameter
    uint32_t block_size;         // Processing block size
};

/**
 * @brief Individual Channel State
 *
 * Tracks state for each output channel
 */
struct pfb_channel_t {
    float complex *buffer;       // Channel output buffer
    uint32_t buffer_size;        // Size of output buffer
    uint32_t samples_written;    // Samples written to current buffer
    double center_frequency;     // Channel center frequency (Hz)
    double bandwidth;            // Channel bandwidth (Hz)
    uint32_t channel_index;      // Channel index (0 to num_channels-1)
};

/**
 * @brief Main PFB Context Structure
 *
 * Contains all state information for polyphase filter bank processing
 */
struct pfb_t {
    pfb_config_t config;         // Configuration parameters

    // Filter design
    float *prototype_filter;     // Prototype filter coefficients
    uint32_t filter_length;      // Length of prototype filter

    // Polyphase decomposition
    float **polyphase_filters;   // Polyphase filter branches
    uint32_t num_branches;       // Number of polyphase branches
    uint32_t branch_length;      // Length of each branch

    // FFT processing
    void *fft_plan;             // FFT plan (implementation specific)
    fft_complex_t *fft_input;   // FFT input buffer
    fft_complex_t *fft_output;  // FFT output buffer

    // Channel management
    pfb_channel_t *channels;    // Array of channel states
    uint32_t num_channels;      // Number of active channels

    // Processing state
    fft_complex_t *input_buffer; // Input sample buffer
    uint32_t input_buffer_size;  // Size of input buffer
    uint32_t input_index;        // Current input buffer position

    // Memory management
    bool initialized;           // Initialization flag
};

/**
 * @brief Initialize PFB Configuration with defaults
 *
 * @param config Pointer to configuration structure to initialize
 * @param num_channels Number of output channels
 * @param sample_rate Input sample rate in Hz
 * @param channel_bandwidth Bandwidth per channel in Hz
 * @return true on success, false on error
 */
bool pfb_config_init(pfb_config_t *config, uint32_t num_channels,
                    double sample_rate, double channel_bandwidth);

/**
 * @brief Validate PFB configuration parameters
 *
 * @param config Pointer to configuration structure
 * @return true if valid, false if invalid
 */
bool pfb_config_validate(const pfb_config_t *config);

/**
 * @brief Create PFB channelizer instance
 *
 * @param config Pointer to validated configuration
 * @return Pointer to initialized PFB instance, NULL on error
 */
pfb_t *pfb_create(const pfb_config_t *config);

/**
 * @brief Destroy PFB channelizer instance
 *
 * @param pfb Pointer to PFB instance
 */
void pfb_destroy(pfb_t *pfb);

/**
 * @brief Process a block of IQ samples through the PFB
 *
 * @param pfb Pointer to PFB instance
 * @param input Complex input samples (I + j*Q)
 * @param input_length Number of input samples
 * @return Number of samples processed, negative on error
 */
int32_t pfb_process_block(pfb_t *pfb, const float complex *input, uint32_t input_length);

/**
 * @brief Get channel output buffer
 *
 * @param pfb Pointer to PFB instance
 * @param channel_index Channel index (0 to num_channels-1)
 * @param samples Pointer to receive number of available samples
 * @return Pointer to channel output buffer, NULL on error
 */
const float complex *pfb_get_channel_output(const pfb_t *pfb, uint32_t channel_index,
                                          uint32_t *samples);

/**
 * @brief Reset channel output buffer (mark as consumed)
 *
 * @param pfb Pointer to PFB instance
 * @param channel_index Channel index (0 to num_channels-1)
 * @return true on success, false on error
 */
bool pfb_reset_channel_output(pfb_t *pfb, uint32_t channel_index);

/**
 * @brief Get PFB processing statistics
 *
 * @param pfb Pointer to PFB instance
 * @param total_samples Pointer to receive total samples processed
 * @param channel_samples Array to receive samples per channel
 * @param channel_samples_size Size of channel_samples array
 * @return true on success, false on error
 */
bool pfb_get_statistics(const pfb_t *pfb, uint64_t *total_samples,
                       uint64_t *channel_samples, uint32_t channel_samples_size);

/**
 * @brief Calculate expected channel isolation for given configuration
 *
 * @param config Pointer to configuration
 * @return Expected channel isolation in dB, negative on error
 */
double pfb_calculate_isolation(const pfb_config_t *config);

/**
 * @brief Get channel center frequency
 *
 * @param pfb Pointer to PFB instance
 * @param channel_index Channel index (0 to num_channels-1)
 * @return Center frequency in Hz, negative on error
 */
double pfb_get_channel_frequency(const pfb_t *pfb, uint32_t channel_index);

/**
 * @brief Get channel bandwidth
 *
 * @param pfb Pointer to PFB instance
 * @param channel_index Channel index (0 to num_channels-1)
 * @return Bandwidth in Hz, negative on error
 */
double pfb_get_channel_bandwidth(const pfb_t *pfb, uint32_t channel_index);

/**
 * @brief Get string representation of PFB configuration
 *
 * @param pfb Pointer to PFB instance
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return true on success, false on error
 */
bool pfb_get_config_string(const pfb_t *pfb, char *buffer, uint32_t buffer_size);

/**
 * @brief Reset PFB processing state
 *
 * @param pfb Pointer to PFB instance
 * @return true on success, false on error
 */
bool pfb_reset(pfb_t *pfb);

#endif /* PFB_H */
