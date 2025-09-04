/*
 * =============================================================================
 * IQ Lab - PNG Image Generator Tool
 * =============================================================================
 *
 * PURPOSE:
 *   Generates PNG spectrum and waterfall visualizations from IQ (In-phase/Quadrature) data
 *   for RF signal analysis and visualization. Perfect for analyzing SDR recordings,
 *   radio signals, and spectrum monitoring.
 *
 * INPUTS:
 *   - IQ data file: WAV format IQ recordings (typically from KiwiSDR)
 *   - Sample rate: Auto-detected or configurable
 *   - Data format: 8-bit or 16-bit IQ samples (I,Q interleaved)
 *
 * OUTPUTS:
 *   - Spectrum PNG: Frequency vs Power (dB) plot
 *     * X-axis: Frequency (centered on carrier)
 *     * Y-axis: Power in dBFS (decibels relative to full scale)
 *     * Color: Power level (blue=low, red=high)
 *
 *   - Waterfall PNG: Frequency vs Time plot
 *     * X-axis: Frequency (centered on carrier)
 *     * Y-axis: Time (newest data at top)
 *     * Color: Power level over time
 *
 * USAGE:
 *   ./generate_images
 *
 *   The tool automatically:
 *   1. Loads IQ data from 'data/kiwi_recording_*.iq.wav'
 *   2. Performs FFT analysis with 75% overlap
 *   3. Generates spectrum with calibrated axes
 *   4. Generates waterfall with time progression
 *   5. Saves PNG files with descriptive names
 *
 * FEATURES:
 *   - Automatic power scaling and color mapping
 *   - Calibrated frequency and dB axes
 *   - FFT shift for proper DC centering
 *   - Progress reporting during processing
 *   - Memory-efficient block processing
 *
 * =============================================================================
 */

#include "../src/viz/img_png.h"
#include "../src/viz/draw_axes.h"
#include "../src/iq_core/io_iq.h"
#include "../src/iq_core/fft.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Configuration structure for image generation
typedef struct {
    uint32_t fft_size;
    uint32_t axis_margin;
    uint8_t axis_color_r;
    uint8_t axis_color_g;
    uint8_t axis_color_b;
    uint32_t max_frames;
} image_config_t;

// Default configuration values
static const image_config_t DEFAULT_CONFIG = {
    .fft_size = 1024,
    .axis_margin = 60,
    .axis_color_r = 255,  // Bright cyan for good visibility
    .axis_color_g = 200,
    .axis_color_b = 0,
    .max_frames = 10000
};

// Get configuration (could be extended to read from file/environment)
static const image_config_t *get_image_config(void) {
    return &DEFAULT_CONFIG;
}

// Function to create proper waterfall from IQ data
bool create_waterfall_from_iq(const iq_data_t *iq_data, png_image_t *waterfall,
                             uint32_t width, uint32_t height, uint32_t axis_margin) {
    if (!iq_data || !waterfall || !iq_data->data || iq_data->num_samples == 0) {
        return false;
    }

    // Get configuration
    const image_config_t *config = get_image_config();

    // Create FFT plan
    fft_plan_t *fft_plan = fft_plan_create(config->fft_size, FFT_FORWARD);
    if (!fft_plan) {
        printf("❌ Failed to create FFT plan for waterfall\n");
        return false;
    }

    // DIFFERENT PARAMETERS for waterfall (matches Python waterfall approach)
    uint32_t hop_size = config->fft_size / 4;  // 75% overlap

    // Prevent integer overflow and ensure valid calculation
    if (iq_data->num_samples <= config->fft_size) {
        printf("❌ Not enough samples for waterfall FFT analysis\n");
        fft_plan_destroy(fft_plan);
        return false;
    }

    // Safe calculation to prevent integer overflow
    uint64_t available_samples = (uint64_t)iq_data->num_samples - config->fft_size;
    uint32_t num_frames = (uint32_t)(available_samples / hop_size) + 1;

    // Limit frames to prevent excessive memory usage
    if (num_frames > config->max_frames) {
        num_frames = config->max_frames;
    }

    if (num_frames == 0) {
        printf("❌ Not enough samples for waterfall FFT analysis\n");
        fft_plan_destroy(fft_plan);
        return false;
    }

    // Limit frames to fit image height
    if (num_frames > height) {
        num_frames = height;
    }

    // Allocate buffers
    fft_complex_t *fft_input = (fft_complex_t *)malloc(config->fft_size * sizeof(fft_complex_t));
    fft_complex_t *fft_output = (fft_complex_t *)malloc(config->fft_size * sizeof(fft_complex_t));
    double *power_spectrum = (double *)malloc(config->fft_size * sizeof(double));

    if (!fft_input || !fft_output || !power_spectrum) {
        printf("❌ Memory allocation failed for waterfall\n");
        free(fft_input);
        free(fft_output);
        free(power_spectrum);
        fft_plan_destroy(fft_plan);
        return false;
    }

    // Find global min/max for consistent color scaling
    double global_min_db = 0.0;
    double global_max_db = -200.0;

    // First pass: find min/max power
    for (uint32_t frame = 0; frame < num_frames; frame++) {
        uint32_t sample_offset = frame * hop_size;

        for (uint32_t i = 0; i < config->fft_size; i++) {
            uint32_t sample_idx = sample_offset + i;
            if (sample_idx >= iq_data->num_samples) {
                fft_input[i] = 0.0 + 0.0 * I;
            } else {
                float i_val = iq_data->data[sample_idx * 2];
                float q_val = iq_data->data[sample_idx * 2 + 1];
                fft_input[i] = i_val + q_val * I;
            }
        }



        if (!fft_execute(fft_plan, fft_input, fft_output)) {
            continue;
        }

        if (!fft_power_spectrum(fft_output, power_spectrum, config->fft_size, true)) {
            continue;
        }

        for (uint32_t i = 0; i < config->fft_size; i++) {
            double power_db = 10.0 * log10(power_spectrum[i] + 1e-12);
            if (power_db < global_min_db) global_min_db = power_db;
            if (power_db > global_max_db) global_max_db = power_db;
        }
    }

    // Second pass: create waterfall image (time flows upward)
    for (uint32_t frame = 0; frame < num_frames; frame++) {
        uint32_t sample_offset = frame * hop_size;

        // Convert IQ samples to complex
        for (uint32_t i = 0; i < config->fft_size; i++) {
            uint32_t sample_idx = sample_offset + i;
            if (sample_idx >= iq_data->num_samples) {
                fft_input[i] = 0.0 + 0.0 * I;
            } else {
                float i_val = iq_data->data[sample_idx * 2];
                float q_val = iq_data->data[sample_idx * 2 + 1];
                fft_input[i] = i_val + q_val * I;
            }
        }



        // Execute FFT
        if (!fft_execute(fft_plan, fft_input, fft_output)) {
            continue;
        }

        // Validate FFT output for waterfall
        double max_magnitude_wf = 0.0;
        for (uint32_t i = 0; i < config->fft_size; i++) {
            double mag = cabs(fft_output[i]);
            if (mag > max_magnitude_wf) max_magnitude_wf = mag;
        }

        if (max_magnitude_wf < 1e-6) {
            printf("⚠️  Waterfall FFT output is near zero at frame %u (max magnitude: %e)\n", frame, max_magnitude_wf);
        }

        // Apply FFT shift to center DC component (matches Python reference)
        fft_complex_t *fft_shifted_wf = (fft_complex_t *)malloc(config->fft_size * sizeof(fft_complex_t));
        if (!fft_shifted_wf) {
            printf("❌ Memory allocation failed for waterfall FFT shift\n");
            continue;
        }

        if (!fft_shift(fft_output, fft_shifted_wf, config->fft_size)) {
            printf("❌ Waterfall FFT shift failed\n");
            free(fft_shifted_wf);
            continue;
        }

        // Calculate power spectrum from shifted FFT
        if (!fft_power_spectrum(fft_shifted_wf, power_spectrum, config->fft_size, true)) {
            free(fft_shifted_wf);
            continue;
        }

        free(fft_shifted_wf);

        // Map to image (newest data at bottom, time flows upward)
        // Use proper scaling to avoid gaps due to integer division
        // IMPORTANT: Leave space for axes on bottom (frequency) AND left (time axis)
        uint32_t graph_width = width;  // Full width for frequency axis on bottom
        uint32_t graph_height = height - axis_margin;  // Leave space for bottom axis (frequency axis)
        uint32_t graph_left = axis_margin;  // Leave space for left axis (time axis)

        // Plot from top of graph area downward (time flows from bottom to top)
        // graph_height pixels available, from y = 0 to y = graph_height - 1
        // Waterfall convention: newest data at top (y=0), oldest data at bottom (y=graph_height-1)
        uint32_t pixels_per_frame = graph_height / num_frames;
        if (pixels_per_frame == 0) pixels_per_frame = 1;

        // For waterfall: newest data (frame=max) at top, oldest data (frame=0) at bottom
        uint32_t start_y = (num_frames - 1 - frame) * pixels_per_frame;
        uint32_t end_y = (num_frames - frame) * pixels_per_frame;

        // Ensure valid range within graph area
        if (start_y >= graph_height) start_y = graph_height - 1;
        if (end_y > graph_height) end_y = graph_height;
        if (end_y <= start_y) end_y = start_y + 1;

        // Fill all pixels in the range for this frame
        for (uint32_t image_y = start_y; image_y < end_y && image_y < height; image_y++) {
            for (uint32_t x = graph_left; x < graph_width; x++) {
                // Map image x-coordinate to FFT bin (relative to graph area)
                uint32_t graph_x = x - graph_left;  // 0 to graph_width-1
                uint32_t fft_bin = graph_x * config->fft_size / graph_width;

                if (fft_bin < config->fft_size) {
                    // DIFFERENT dB scaling for waterfall (matches Python waterfall: 20*log10 vs 10*log10)
                    double power_db = 20.0 * log10(power_spectrum[fft_bin] + 1e-12);
                    double normalized = (power_db - global_min_db) / (global_max_db - global_min_db + 1e-12);

                    if (normalized < 0.0) normalized = 0.0;
                    if (normalized > 1.0) normalized = 1.0;

                    uint8_t r, g, b;
                    png_intensity_to_color((float)normalized, &r, &g, &b);
                    png_image_set_pixel(waterfall, x, image_y, r, g, b);
                }
            }
        }
    }

    // Cleanup
    free(fft_input);
    free(fft_output);
    free(power_spectrum);
    fft_plan_destroy(fft_plan);

    return true;
}

int main(void) {
    printf("🎯 IQ Lab Image Generator\n");
    printf("=======================\n\n");

    // Get configuration
    const image_config_t *config = get_image_config();

    // Load IQ data from WAV file
    printf("📡 Loading IQ data from Kiwi SDR recording...\n");
    iq_data_t iq_data = {0};

    if (!iq_load_file("data/kiwi.wav", &iq_data)) {
        printf("❌ Failed to load WAV file\n");
        return 1;
    }

    // Set sample rate for raw IQ files (they don't contain this metadata)
    if (iq_data.sample_rate == 0) {
        iq_data.sample_rate = 12000; // Default KiwiSDR rate
    }

    printf("✅ Loaded %zu samples at %u Hz sample rate\n", iq_data.num_samples, iq_data.sample_rate);
    printf("📊 Data format: %s\n", iq_data.format == IQ_FORMAT_S8 ? "8-bit" : "16-bit");

    // Image dimensions
    const uint32_t width = 1024;
    const uint32_t height = 512;

    // Define center frequency for axis labeling
    double center_freq = 100000000.0; // 100 MHz center frequency

    // FFT setup for all image generation
    uint32_t hop_size = config->fft_size / 4;  // 75% overlap
    uint32_t num_frames = (iq_data.num_samples - config->fft_size) / hop_size + 1;

    if (num_frames == 0) {
        printf("❌ Not enough samples for FFT analysis\n");
        iq_free(&iq_data);
        return 1;
    }

    // Limit frames to fit image height
    if (num_frames > height) {
        num_frames = height;
    }

    // Allocate shared FFT buffers
    fft_plan_t *fft_plan = fft_plan_create(config->fft_size, FFT_FORWARD);
    if (!fft_plan) {
        printf("❌ Failed to create FFT plan\n");
        iq_free(&iq_data);
        return 1;
    }

    fft_complex_t *fft_input = (fft_complex_t *)malloc(config->fft_size * sizeof(fft_complex_t));
    fft_complex_t *fft_output = (fft_complex_t *)malloc(config->fft_size * sizeof(fft_complex_t));
    fft_complex_t *fft_shifted = (fft_complex_t *)malloc(config->fft_size * sizeof(fft_complex_t));
    double *power_spectrum = (double *)malloc(config->fft_size * sizeof(double));

    if (!fft_input || !fft_output || !fft_shifted || !power_spectrum) {
        printf("❌ Memory allocation failed\n");
        free(fft_input);
        free(fft_output);
        free(fft_shifted);
        free(power_spectrum);
        fft_plan_destroy(fft_plan);
        iq_free(&iq_data);
        return 1;
    }

    // Find global min/max power for all plots
    double global_min_db = 0.0;
    double global_max_db = -200.0;

    // Pre-calculate global min/max for consistent color scaling
    for (uint32_t frame = 0; frame < num_frames; frame++) {
        uint32_t sample_offset = frame * hop_size;

        for (uint32_t i = 0; i < config->fft_size; i++) {
            uint32_t sample_idx = sample_offset + i;
            if (sample_idx >= iq_data.num_samples) {
                fft_input[i] = 0.0 + 0.0 * I;
            } else {
                float i_val = iq_data.data[sample_idx * 2];
                float q_val = iq_data.data[sample_idx * 2 + 1];
                fft_input[i] = i_val + q_val * I;
            }
        }

        if (!fft_execute(fft_plan, fft_input, fft_output)) {
            continue;
        }

        if (!fft_shift(fft_output, fft_shifted, config->fft_size)) {
            continue;
        }

        if (!fft_power_spectrum(fft_shifted, power_spectrum, config->fft_size, true)) {
            continue;
        }

        for (uint32_t i = 0; i < config->fft_size; i++) {
            double power_db = 10.0 * log10(power_spectrum[i] + 1e-12);
            if (power_db < global_min_db) global_min_db = power_db;
            if (power_db > global_max_db) global_max_db = power_db;
        }
    }

    // Create spectrum (frequency vs power) - IMPROVED VERSION
    printf("\n🎨 Generating spectrum (frequency vs power)...\n");

    png_image_t spectrum;
    if (!png_image_init(&spectrum, width, height)) {
        printf("❌ Failed to create spectrum image\n");
        iq_free(&iq_data);
        return 1;
    }

    // Fill with dark background
    png_image_fill(&spectrum, 10, 10, 10);

    // Draw axes first (before data)
    draw_db_scale(spectrum.data, width, height, config->axis_margin,
                 global_min_db, global_max_db, config->axis_color_r, config->axis_color_g, config->axis_color_b);

    draw_frequency_axis(spectrum.data, width, height, config->axis_margin,
                       center_freq, iq_data.sample_rate, config->fft_size,
                       config->axis_color_r, config->axis_color_g, config->axis_color_b);

    // Generate spectrum using dedicated FFT resources
    {
        // Create separate FFT resources for spectrum generation
        fft_plan_t *spectrum_plan = fft_plan_create(config->fft_size, FFT_FORWARD);
        fft_complex_t *spectrum_input = (fft_complex_t *)malloc(config->fft_size * sizeof(fft_complex_t));
        fft_complex_t *spectrum_output = (fft_complex_t *)malloc(config->fft_size * sizeof(fft_complex_t));
        fft_complex_t *spectrum_shifted = (fft_complex_t *)malloc(config->fft_size * sizeof(fft_complex_t));
        double *spectrum_power = (double *)malloc(config->fft_size * sizeof(double));

        if (!spectrum_plan || !spectrum_input || !spectrum_output || !spectrum_shifted || !spectrum_power) {
            printf("❌ Failed to allocate memory for spectrum generation\n");
            if (spectrum_plan) fft_plan_destroy(spectrum_plan);
            free(spectrum_input);
            free(spectrum_output);
            free(spectrum_shifted);
            free(spectrum_power);
            png_image_free(&spectrum);
            iq_free(&iq_data);
            return 1;
        }

        // Use first frame only for spectrum
        uint32_t frame = 0;
        uint32_t sample_offset = frame * hop_size;

        for (uint32_t i = 0; i < config->fft_size; i++) {
            uint32_t sample_idx = sample_offset + i;
            if (sample_idx >= iq_data.num_samples) {
                spectrum_input[i] = 0.0 + 0.0 * I;
            } else {
                float i_val = iq_data.data[sample_idx * 2];
                float q_val = iq_data.data[sample_idx * 2 + 1];
                spectrum_input[i] = i_val + q_val * I;
            }
        }

        if (!fft_execute(spectrum_plan, spectrum_input, spectrum_output) ||
            !fft_shift(spectrum_output, spectrum_shifted, config->fft_size) ||
            !fft_power_spectrum(spectrum_shifted, spectrum_power, config->fft_size, true)) {
            printf("❌ Spectrum FFT processing failed\n");
        } else {
            // Map to spectrum image
            for (uint32_t x = config->axis_margin; x < width; x++) {
                uint32_t bin = ((x - config->axis_margin) * config->fft_size) / (width - config->axis_margin);
                if (bin >= config->fft_size) continue;

                double power_db = 10.0 * log10(spectrum_power[bin] + 1e-12);
                double normalized = (power_db - global_min_db) / (global_max_db - global_min_db + 1e-12);
                if (normalized < 0.0) normalized = 0.0;
                if (normalized > 1.0) normalized = 1.0;

                uint8_t r, g, b;
                png_intensity_to_color((float)normalized, &r, &g, &b);

                // Draw spectrum bars from top margin to bottom margin (leave space for axes)
                for (uint32_t y = config->axis_margin; y < height - config->axis_margin; y++) {
                    png_image_set_pixel(&spectrum, x, y, r, g, b);
                }
            }
        }

        // Clean up spectrum-specific resources
        fft_plan_destroy(spectrum_plan);
        free(spectrum_input);
        free(spectrum_output);
        free(spectrum_shifted);
        free(spectrum_power);
    }

    printf("📊 Power range: %.1f dB to %.1f dB\n", global_min_db, global_max_db);

    // Save spectrum
    if (png_image_write(&spectrum, "kiwi_spectrum_fixed.png")) {
        printf("✅ Saved spectrum: kiwi_spectrum_fixed.png\n");
    } else {
        printf("❌ Failed to save spectrum\n");
    }

    png_image_free(&spectrum);

    // Create waterfall
    printf("\n🌊 Generating waterfall...\n");

    png_image_t waterfall;
    if (!png_image_init(&waterfall, width, height)) {
        printf("❌ Failed to create waterfall image\n");
        iq_free(&iq_data);
        return 1;
    }

    // Fill with dark background
    png_image_fill(&waterfall, 10, 10, 10);

    // Generate waterfall
    if (!create_waterfall_from_iq(&iq_data, &waterfall, width, height, config->axis_margin)) {
        printf("❌ Failed to generate waterfall\n");
        png_image_free(&waterfall);
        iq_free(&iq_data);
        return 1;
    }

    // Add axes
    printf("📐 Adding axes...\n");

    // Time axis on left (y-axis) - DRAW FIRST
    double total_duration = (double)iq_data.num_samples / iq_data.sample_rate;
    draw_time_axis(waterfall.data, width, height, config->axis_margin,
                  total_duration, 0,  // time_slices not used
                  config->axis_color_r, config->axis_color_g, config->axis_color_b);

    // Frequency axis on bottom (x-axis) - DRAW LAST
    draw_frequency_axis(waterfall.data, width, height, config->axis_margin,
                       6000000.0, iq_data.sample_rate, config->fft_size,  // 6 MHz center freq
                       config->axis_color_r, config->axis_color_g, config->axis_color_b);

    // Save waterfall
    if (png_image_write(&waterfall, "kiwi_waterfall_fixed.png")) {
        printf("✅ Saved waterfall: kiwi_waterfall_fixed.png\n");
    } else {
        printf("❌ Failed to save waterfall\n");
    }

    png_image_free(&waterfall);

    // Final cleanup - free IQ data
    iq_free(&iq_data);

    printf("\n🎉 Image generation completed successfully!\n");
    printf("📁 Generated files:\n");
    printf("   • kiwi_spectrum_fixed.png - Spectrum (frequency vs power)\n");
    printf("   • kiwi_waterfall_fixed.png - Waterfall (frequency vs time)\n");

    return 0;
}