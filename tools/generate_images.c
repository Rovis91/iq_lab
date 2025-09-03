/*
 * =============================================================================
 * IQ Lab - PNG Image Generator Tool
 * =============================================================================
 *
 * PURPOSE:
 *   Generates PNG spectrograms and waterfalls from IQ (In-phase/Quadrature) data
 *   for RF signal analysis and visualization. Perfect for analyzing SDR recordings,
 *   radio signals, and spectrum monitoring.
 *
 * INPUTS:
 *   - IQ data file: WAV format IQ recordings (typically from KiwiSDR)
 *   - Sample rate: Auto-detected or configurable
 *   - Data format: 8-bit or 16-bit IQ samples (I,Q interleaved)
 *
 * OUTPUTS:
 *   - Spectrogram PNG: Frequency vs Power (dB) plot
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
 *   3. Generates spectrogram with calibrated axes
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

#include "src/viz/img_png.h"
#include "src/viz/img_ppm.h"
#include "src/viz/draw_axes.h"
#include "src/iq_core/io_iq.h"
#include "src/iq_core/fft.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Fixed axis colors for better visibility
#define AXIS_COLOR_R 255  // Bright cyan for good visibility
#define AXIS_COLOR_G 200
#define AXIS_COLOR_B 0

// Fixed margin size
#define AXIS_MARGIN 60

// FFT size for spectrogram analysis
#define FFT_SIZE 1024

// Function to create proper spectrogram from IQ data
bool create_spectrogram_from_iq(const iq_data_t *iq_data, png_image_t *spectrogram,
                               uint32_t width, uint32_t height, uint32_t axis_margin,
                               double *min_db, double *max_db) {
    if (!iq_data || !spectrogram || !iq_data->data || iq_data->num_samples == 0) {
        return false;
    }

    // Create FFT plan
    fft_plan_t *fft_plan = fft_plan_create(FFT_SIZE, FFT_FORWARD);
    if (!fft_plan) {
        printf("❌ Failed to create FFT plan\n");
        return false;
    }

    // Calculate parameters
    uint32_t hop_size = FFT_SIZE / 4;  // 75% overlap
    uint32_t num_frames = (iq_data->num_samples - FFT_SIZE) / hop_size + 1;

    if (num_frames == 0) {
        printf("❌ Not enough samples for FFT analysis\n");
        fft_plan_destroy(fft_plan);
        return false;
    }

    // Limit frames to fit image height
    if (num_frames > height) {
        num_frames = height;
    }

    // Allocate buffers
    fft_complex_t *fft_input = (fft_complex_t *)malloc(FFT_SIZE * sizeof(fft_complex_t));
    fft_complex_t *fft_output = (fft_complex_t *)malloc(FFT_SIZE * sizeof(fft_complex_t));
    double *power_spectrum = (double *)malloc(FFT_SIZE * sizeof(double));

    if (!fft_input || !fft_output || !power_spectrum) {
        printf("❌ Memory allocation failed\n");
        free(fft_input);
        free(fft_output);
        free(power_spectrum);
        fft_plan_destroy(fft_plan);
        return false;
    }

    // Find min/max power for normalization
    double global_min_db = 0.0;
    double global_max_db = -200.0;

    // Process each time frame
    for (uint32_t frame = 0; frame < num_frames; frame++) {
        uint32_t sample_offset = frame * hop_size;

        // Convert IQ samples to complex format
        for (uint32_t i = 0; i < FFT_SIZE; i++) {
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
            printf("❌ FFT execution failed at frame %u\n", frame);
            continue;
        }

        // Validate FFT output (check for non-zero results)
        double max_magnitude = 0.0;
        for (uint32_t i = 0; i < FFT_SIZE; i++) {
            double mag = cabs(fft_output[i]);
            if (mag > max_magnitude) max_magnitude = mag;
        }

        if (max_magnitude < 1e-6) {
            printf("⚠️  FFT output is near zero at frame %u (max magnitude: %e)\n", frame, max_magnitude);
        }

        // Apply FFT shift to center DC component (matches Python scipy.signal.spectrogram)
        fft_complex_t *fft_shifted = (fft_complex_t *)malloc(FFT_SIZE * sizeof(fft_complex_t));
        if (!fft_shifted) {
            printf("❌ Memory allocation failed for FFT shift\n");
            continue;
        }

        if (!fft_shift(fft_output, fft_shifted, FFT_SIZE)) {
            printf("❌ FFT shift failed\n");
            free(fft_shifted);
            continue;
        }

        // Calculate power spectrum from shifted FFT
        if (!fft_power_spectrum(fft_shifted, power_spectrum, FFT_SIZE, true)) {
            printf("❌ Power spectrum calculation failed\n");
            free(fft_shifted);
            continue;
        }

        free(fft_shifted);

        // Convert to dB and find min/max
        for (uint32_t i = 0; i < FFT_SIZE; i++) {
            double power_db = 10.0 * log10(power_spectrum[i] + 1e-12);
            if (power_db < global_min_db) global_min_db = power_db;
            if (power_db > global_max_db) global_max_db = power_db;
        }

        // Map FFT frames to image pixels (ensure all pixels are filled)
        // Use proper scaling to avoid gaps due to integer division
        // IMPORTANT: Leave space for axes on bottom (frequency) AND left (dB scale)
        uint32_t graph_width = width;  // Full width for frequency axis on bottom
        uint32_t graph_height = height - axis_margin;  // Leave space for bottom axis
        uint32_t graph_left = axis_margin;  // Leave space for left axis (dB scale)

        uint32_t start_y = (frame * graph_height) / num_frames;
        uint32_t end_y = ((frame + 1) * graph_height) / num_frames;

        // Ensure we fill at least one pixel per frame
        if (start_y >= graph_height) start_y = graph_height - 1;
        if (end_y > graph_height) end_y = graph_height;
        if (end_y <= start_y) end_y = start_y + 1;

        // Fill all pixels in the range for this frame
        for (uint32_t image_y = start_y; image_y < end_y && image_y < graph_height; image_y++) {
            for (uint32_t x = graph_left; x < graph_width; x++) {
                // Map image x-coordinate to FFT bin (relative to graph area)
                uint32_t graph_x = x - graph_left;  // 0 to graph_width-1
                uint32_t fft_bin = graph_x * FFT_SIZE / graph_width;

                if (fft_bin < FFT_SIZE) {
                    double power_db = 10.0 * log10(power_spectrum[fft_bin] + 1e-12);

                    // Normalize to 0-1 range
                    double normalized = (power_db - global_min_db) / (global_max_db - global_min_db + 1e-12);
                    if (normalized < 0.0) normalized = 0.0;
                    if (normalized > 1.0) normalized = 1.0;

                    // Apply color mapping
                    uint8_t r, g, b;
                    png_intensity_to_color((float)normalized, &r, &g, &b);
                    png_image_set_pixel(spectrogram, x, image_y, r, g, b);
                }
            }
        }

        if (frame % 50 == 0) {
            printf("  Processed %u/%u frames (%.1f%%)\n",
                   frame + 1, num_frames, (frame + 1) * 100.0 / num_frames);
        }
    }

    // Return min/max for axis scaling
    if (min_db) *min_db = global_min_db;
    if (max_db) *max_db = global_max_db;

    // Cleanup
    free(fft_input);
    free(fft_output);
    free(power_spectrum);
    fft_plan_destroy(fft_plan);

    return true;
}

// Function to create proper waterfall from IQ data
bool create_waterfall_from_iq(const iq_data_t *iq_data, png_image_t *waterfall,
                             uint32_t width, uint32_t height, uint32_t axis_margin) {
    if (!iq_data || !waterfall || !iq_data->data || iq_data->num_samples == 0) {
        return false;
    }

    // Create FFT plan
    fft_plan_t *fft_plan = fft_plan_create(FFT_SIZE, FFT_FORWARD);
    if (!fft_plan) {
        printf("❌ Failed to create FFT plan for waterfall\n");
        return false;
    }

    // DIFFERENT PARAMETERS for waterfall (matches Python waterfall approach)
    uint32_t hop_size = FFT_SIZE / 4;  // 75% overlap (different from spectrogram)
    uint32_t num_frames = (iq_data->num_samples - FFT_SIZE) / hop_size + 1;

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
    fft_complex_t *fft_input = (fft_complex_t *)malloc(FFT_SIZE * sizeof(fft_complex_t));
    fft_complex_t *fft_output = (fft_complex_t *)malloc(FFT_SIZE * sizeof(fft_complex_t));
    double *power_spectrum = (double *)malloc(FFT_SIZE * sizeof(double));

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

        for (uint32_t i = 0; i < FFT_SIZE; i++) {
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

        if (!fft_power_spectrum(fft_output, power_spectrum, FFT_SIZE, true)) {
            continue;
        }

        for (uint32_t i = 0; i < FFT_SIZE; i++) {
            double power_db = 10.0 * log10(power_spectrum[i] + 1e-12);
            if (power_db < global_min_db) global_min_db = power_db;
            if (power_db > global_max_db) global_max_db = power_db;
        }
    }

    // Second pass: create waterfall image (time flows upward)
    for (uint32_t frame = 0; frame < num_frames; frame++) {
        uint32_t sample_offset = frame * hop_size;

        // Convert IQ samples to complex
        for (uint32_t i = 0; i < FFT_SIZE; i++) {
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
        for (uint32_t i = 0; i < FFT_SIZE; i++) {
            double mag = cabs(fft_output[i]);
            if (mag > max_magnitude_wf) max_magnitude_wf = mag;
        }

        if (max_magnitude_wf < 1e-6) {
            printf("⚠️  Waterfall FFT output is near zero at frame %u (max magnitude: %e)\n", frame, max_magnitude_wf);
        }

        // Apply FFT shift to center DC component (matches Python reference)
        fft_complex_t *fft_shifted_wf = (fft_complex_t *)malloc(FFT_SIZE * sizeof(fft_complex_t));
        if (!fft_shifted_wf) {
            printf("❌ Memory allocation failed for waterfall FFT shift\n");
            continue;
        }

        if (!fft_shift(fft_output, fft_shifted_wf, FFT_SIZE)) {
            printf("❌ Waterfall FFT shift failed\n");
            free(fft_shifted_wf);
            continue;
        }

        // Calculate power spectrum from shifted FFT
        if (!fft_power_spectrum(fft_shifted_wf, power_spectrum, FFT_SIZE, true)) {
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
                uint32_t fft_bin = graph_x * FFT_SIZE / graph_width;

                if (fft_bin < FFT_SIZE) {
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

    // Load IQ data from WAV file
    printf("📡 Loading IQ data from Kiwi SDR recording...\n");
    iq_data_t iq_data = {0};

    if (!iq_load_file("data/kiwi_recording_20250902_221135.iq.wav", &iq_data)) {
        printf("❌ Failed to load WAV file\n");
        return 1;
    }

    if (iq_data.sample_rate == 0) {
        iq_data.sample_rate = 12000; // Default KiwiSDR rate
    }

    printf("✅ Loaded %zu samples at %u Hz sample rate\n", iq_data.num_samples, iq_data.sample_rate);
    printf("📊 Data format: %s\n", iq_data.format == IQ_FORMAT_S8 ? "8-bit" : "16-bit");

    // Create spectrogram
    printf("\n🎨 Generating spectrogram...\n");

    const uint32_t width = 1024;
    const uint32_t height = 512;

    png_image_t spectrogram;
    if (!png_image_init(&spectrogram, width, height)) {
        printf("❌ Failed to create spectrogram image\n");
        iq_free(&iq_data);
        return 1;
    }

    // Fill with dark background
    png_image_fill(&spectrogram, 10, 10, 10);

    // Generate spectrogram
    double min_db = 0.0, max_db = 0.0;
    if (!create_spectrogram_from_iq(&iq_data, &spectrogram, width, height, AXIS_MARGIN, &min_db, &max_db)) {
        printf("❌ Failed to generate spectrogram\n");
        png_image_free(&spectrogram);
        iq_free(&iq_data);
        return 1;
    }

    printf("📊 Power range: %.1f dB to %.1f dB\n", min_db, max_db);

    // Add axes
    printf("📐 Adding axes...\n");

    // dB scale on left (y-axis) - DRAW FIRST
    draw_db_scale(spectrogram.data, width, height, AXIS_MARGIN,
                 min_db, max_db, AXIS_COLOR_R, AXIS_COLOR_G, AXIS_COLOR_B);

    // Frequency axis on bottom (x-axis) - DRAW LAST
    draw_frequency_axis(spectrogram.data, width, height, AXIS_MARGIN,
                       6000000.0, iq_data.sample_rate, FFT_SIZE,  // 6 MHz center freq
                       AXIS_COLOR_R, AXIS_COLOR_G, AXIS_COLOR_B);

    // Save spectrogram
    if (png_image_write(&spectrogram, "kiwi_spectrogram_fixed.png")) {
        printf("✅ Saved spectrogram: kiwi_spectrogram_fixed.png\n");
    } else {
        printf("❌ Failed to save spectrogram\n");
    }

    png_image_free(&spectrogram);

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
    if (!create_waterfall_from_iq(&iq_data, &waterfall, width, height, AXIS_MARGIN)) {
        printf("❌ Failed to generate waterfall\n");
        png_image_free(&waterfall);
        iq_free(&iq_data);
        return 1;
    }

    // Add axes
    printf("📐 Adding axes...\n");

    // Time axis on left (y-axis) - DRAW FIRST
    double total_duration = (double)iq_data.num_samples / iq_data.sample_rate;
    draw_time_axis(waterfall.data, width, height, AXIS_MARGIN,
                  total_duration, 0,  // time_slices not used
                  AXIS_COLOR_R, AXIS_COLOR_G, AXIS_COLOR_B);

    // Frequency axis on bottom (x-axis) - DRAW LAST
    draw_frequency_axis(waterfall.data, width, height, AXIS_MARGIN,
                       6000000.0, iq_data.sample_rate, FFT_SIZE,  // 6 MHz center freq
                       AXIS_COLOR_R, AXIS_COLOR_G, AXIS_COLOR_B);

    // Save waterfall
    if (png_image_write(&waterfall, "kiwi_waterfall_fixed.png")) {
        printf("✅ Saved waterfall: kiwi_waterfall_fixed.png\n");
    } else {
        printf("❌ Failed to save waterfall\n");
    }

    png_image_free(&waterfall);
    iq_free(&iq_data);

    printf("\n🎉 Image generation completed successfully!\n");
    printf("📁 Generated files:\n");
    printf("   • kiwi_spectrogram_fixed.png - Spectrogram with frequency (x) and dB (y) axes\n");
    printf("   • kiwi_waterfall_fixed.png - Waterfall with frequency (x) and time (y) axes\n");

    return 0;
}
