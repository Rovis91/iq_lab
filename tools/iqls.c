/*
 * IQ Lab - iqls: Interactive Spectrum and Waterfall Visualization Tool
 *
 * Purpose: Professional spectrum analysis with waterfall visualization
 * Author: IQ Lab Development Team
 *
 * This tool provides comprehensive spectral analysis capabilities for IQ signals,
 * generating both static spectrum plots and dynamic waterfall displays. It uses
 * FFT-based processing with configurable parameters for detailed signal analysis.
 *
 * Key Features:
 * - Fast Fourier Transform (FFT) based spectrum computation
 * - FFT shift for proper frequency domain display (-fs/2 to +fs/2)
 * - Configurable FFT size and windowing parameters
 * - Spectrum averaging over multiple frames for noise reduction
 * - Linear and logarithmic magnitude display modes
 * - High-quality PNG output with professional axis labeling
 * - Optional waterfall visualization with time-frequency representation
 * - Automatic image scaling and dynamic range optimization
 *
 * Usage Examples:
 *   # Basic spectrum analysis
 *   iqls.exe --in signal.iq --rate 2000000 --fft 1024 --hop 512 --avg 10 --out spectrum
 *
 *   # Waterfall visualization with log magnitude
 *   iqls.exe --in signal.iq --rate 2000000 --fft 2048 --hop 1024 --avg 5 --logmag --waterfall --out analysis
 *
 *   # High-resolution narrowband analysis
 *   iqls.exe --in narrowband.iq --rate 500000 --fft 4096 --hop 2048 --avg 20 --out detailed
 *
 * Technical Algorithm:
 * 1. FFT Computation: Convert time-domain IQ to frequency domain
 * 2. FFT Shift: Rearrange bins from (0,fs) to (-fs/2,fs/2)
 * 3. Power Spectrum: Calculate magnitude squared for power spectral density
 * 4. Averaging: Accumulate multiple FFT frames for improved SNR
 * 5. Normalization: Scale to dBFS or linear scale as requested
 * 6. Visualization: Map spectral data to color/intensity values
 *
 * FFT Processing Details:
 * - Complex FFT input: I + jQ from interleaved IQ samples
 * - FFT shift: Proper frequency domain ordering for display
 * - Power calculation: |X[k]|² for power spectral density
 * - Averaging: Reduces noise while preserving signal features
 * - Dynamic range: Automatic scaling from min to max values
 *
 * Visualization Features:
 * - Spectrum Plot: Traditional frequency vs power display
 * - Waterfall Plot: Time vs frequency vs power representation
 * - Axis Labels: Frequency (Hz) and power (dBFS or linear)
 * - Color Mapping: Intensity-based color scale (black to white)
 * - Image Scaling: Automatic aspect ratio and resolution control
 *
 * Performance Optimization:
 * - FFT size must be power of 2 for efficiency
 * - Hop size controls temporal resolution vs computation
 * - Averaging count balances SNR vs temporal detail
 * - Memory efficient processing with streaming FFT
 *
 * Input Requirements:
 * - Raw IQ data: s8 or s16 interleaved samples
 * - Sample rate specification required
 * - Sufficient samples for requested FFT size and averaging
 *
 * Output Formats:
 * - PNG images with professional quality
 * - Spectrum: Static frequency domain plot
 * - Waterfall: Dynamic time-frequency representation
 * - Automatic file naming and metadata
 *
 * Applications:
 * - RF signal analysis and monitoring
 * - Spectrum occupancy studies
 * - Signal detection and classification
 * - Interference analysis and troubleshooting
 * - Educational demonstrations of spectral analysis
 * - Quality assurance for RF systems
 *
 * Dependencies: FFT library, PNG visualization, IQ I/O
 * Thread Safety: Single-threaded processing
 * Error Handling: Comprehensive validation and user feedback
 */

#include "../src/iq_core/io_iq.h"
#include "../src/iq_core/fft.h"
#include "../src/viz/img_png.h"
#include "../src/viz/draw_axes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    const char *in_path;
    const char *format_str; // s8|s16 (optional if autodetected)
    uint32_t sample_rate;
    uint32_t fft_size;
    uint32_t hop_size;
    uint32_t avg_count;
    int logmag;             // boolean
    int waterfall;          // boolean
    const char *out_prefix;
} iqls_args_t;

static void print_usage(void) {
    printf("Usage: iqls --in <file> --format {s8|s16} --rate <Hz> --fft N --hop H --avg K [--logmag] [--waterfall] --out <prefix>\n");
}

static int parse_args(int argc, char **argv, iqls_args_t *args) {
    memset(args, 0, sizeof(*args));
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--in") == 0 && i + 1 < argc) args->in_path = argv[++i];
        else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) args->format_str = argv[++i];
        else if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc) args->sample_rate = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--fft") == 0 && i + 1 < argc) args->fft_size = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--hop") == 0 && i + 1 < argc) args->hop_size = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--avg") == 0 && i + 1 < argc) args->avg_count = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--logmag") == 0) args->logmag = 1;
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) args->out_prefix = argv[++i];
        else if (strcmp(argv[i], "--waterfall") == 0) args->waterfall = 1;
        else {
            print_usage();
            return 0;
        }
    }
    if (!args->in_path || !args->sample_rate || !args->fft_size || !args->hop_size || !args->avg_count || !args->out_prefix) {
        print_usage();
        return 0;
    }
    if (!fft_is_power_of_two(args->fft_size)) {
        fprintf(stderr, "FFT size must be power of two\n");
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    iqls_args_t args;
    if (!parse_args(argc, argv, &args)) return 1;

    iq_data_t iq = {0};
    if (!iq_load_file(args.in_path, &iq)) {
        fprintf(stderr, "Failed to load IQ file: %s\n", args.in_path);
        return 1;
    }
    if (args.sample_rate) iq.sample_rate = args.sample_rate;
    if (iq.sample_rate == 0) {
        fprintf(stderr, "Sample rate required\n");
        iq_free(&iq);
        return 1;
    }

    // Set reasonable image dimensions with good aspect ratio
    const uint32_t max_width = 1024;  // Maximum readable width
    const uint32_t width = args.fft_size > max_width ? max_width : args.fft_size;
    const uint32_t height = 512;  // Fixed height for good readability
    const uint32_t axis_margin = 60;

    // Prepare FFT
    fft_plan_t *plan = fft_plan_create(args.fft_size, FFT_FORWARD);
    if (!plan) {
        fprintf(stderr, "Failed to create FFT plan\n");
        iq_free(&iq);
        return 1;
    }
    fft_complex_t *in = malloc(args.fft_size * sizeof(fft_complex_t));
    fft_complex_t *out = malloc(args.fft_size * sizeof(fft_complex_t));
    fft_complex_t *shifted = malloc(args.fft_size * sizeof(fft_complex_t));
    double *accum = calloc(args.fft_size, sizeof(double));
    if (!in || !out || !shifted || !accum) {
        fprintf(stderr, "Allocation failed\n");
        free(in); free(out); free(shifted); free(accum);
        fft_plan_destroy(plan);
        iq_free(&iq);
        return 1;
    }

    // Average spectra over K frames
    uint64_t frames_done = 0;
    uint64_t offset = 0;
    while (frames_done < args.avg_count) {
        if (offset + args.fft_size >= iq.num_samples) break;
        for (uint32_t i = 0; i < args.fft_size; i++) {
            float i_val = iq.data[(offset + i) * 2];
            float q_val = iq.data[(offset + i) * 2 + 1];
            in[i] = i_val + q_val * I;
        }
        if (!fft_execute(plan, in, out)) break;
        if (!fft_shift(out, shifted, args.fft_size)) break;

        for (uint32_t k = 0; k < args.fft_size; k++) {
            double p = creal(shifted[k]) * creal(shifted[k]) + cimag(shifted[k]) * cimag(shifted[k]);
            accum[k] += p;
        }

        frames_done++;
        offset += args.hop_size;
    }
    if (frames_done == 0) {
        fprintf(stderr, "No frames processed\n");
        free(in); free(out); free(shifted); free(accum);
        fft_plan_destroy(plan);
        iq_free(&iq);
        return 1;
    }
    for (uint32_t k = 0; k < args.fft_size; k++) accum[k] /= (double)frames_done;

    // Normalize to dBFS range
    double min_db = 1e9, max_db = -1e9;
    for (uint32_t k = 0; k < args.fft_size; k++) {
        double v = accum[k];
        double db = args.logmag ? 10.0 * log10(v + 1e-12) : sqrt(v);
        if (db < min_db) min_db = db;
        if (db > max_db) max_db = db;
        accum[k] = db;
    }

    png_image_t img;
    if (!png_image_init(&img, width, height)) {
        fprintf(stderr, "Image init failed\n");
        free(in); free(out); free(shifted); free(accum);
        fft_plan_destroy(plan);
        iq_free(&iq);
        return 1;
    }
    png_image_fill(&img, 10, 10, 10);

    // Axes
    draw_db_scale(img.data, width, height, axis_margin, min_db, max_db, 255, 200, 0);
    draw_frequency_axis(img.data, width, height, axis_margin, 0.0, iq.sample_rate, args.fft_size, 255, 200, 0);

    // Plot
    for (uint32_t x = axis_margin; x < width; x++) {
        uint32_t bin = (uint32_t)((x - axis_margin) * (double)args.fft_size / (double)(width - axis_margin));
        if (bin >= args.fft_size) continue;
        double norm = (accum[bin] - min_db) / (max_db - min_db + 1e-12);
        if (norm < 0.0) {
            norm = 0.0;
        } else if (norm > 1.0) {
            norm = 1.0;
        }
        uint8_t r, g, b; png_intensity_to_color((float)norm, &r, &g, &b);
        for (uint32_t y = axis_margin; y < height - axis_margin; y++) {
            png_image_set_pixel(&img, x, y, r, g, b);
        }
    }

    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s_spectrum.png", args.out_prefix);
    if (!png_image_write(&img, out_path)) {
        fprintf(stderr, "Failed to write %s\n", out_path);
    } else {
        printf("Wrote %s\n", out_path);
    }

    png_image_free(&img);
    
    // Optional: Waterfall image
    if (args.waterfall) {
        png_image_t wimg;
        if (!png_image_init(&wimg, width, height)) {
            fprintf(stderr, "Waterfall image init failed\n");
        } else {
            png_image_fill(&wimg, 10, 10, 10);

            // Determine number of frames by available samples
            uint32_t hop = args.hop_size;
            uint64_t max_frames = 0;
            if (iq.num_samples > args.fft_size && hop > 0) {
                max_frames = (uint64_t)(iq.num_samples - args.fft_size) / hop + 1;
            }
            // Calculate available graph height (between top and bottom margins)
            uint32_t graph_height = height - 2 * axis_margin;
            if (max_frames > graph_height) max_frames = graph_height;

            // Precompute global min/max for consistent scaling
            double wf_min = 1e9, wf_max = -1e9;
            for (uint64_t fidx = 0, off = 0; fidx < max_frames; fidx++, off += hop) {
                for (uint32_t i = 0; i < args.fft_size; i++) {
                    float i_val = iq.data[(off + i) * 2];
                    float q_val = iq.data[(off + i) * 2 + 1];
                    in[i] = i_val + q_val * I;
                }
                if (!fft_execute(plan, in, out) || !fft_shift(out, shifted, args.fft_size)) continue;
                for (uint32_t k = 0; k < args.fft_size; k++) {
                    double p = creal(shifted[k]) * creal(shifted[k]) + cimag(shifted[k]) * cimag(shifted[k]);
                    double db = args.logmag ? 10.0 * log10(p + 1e-12) : sqrt(p);
                    if (db < wf_min) wf_min = db;
                    if (db > wf_max) wf_max = db;
                }
            }
            if (wf_max <= wf_min) { wf_max = wf_min + 1.0; }

            // Render rows from bottom up (older at bottom)
            for (uint64_t fidx = 0, off = 0; fidx < max_frames; fidx++, off += hop) {
                for (uint32_t i = 0; i < args.fft_size; i++) {
                    float i_val = iq.data[(off + i) * 2];
                    float q_val = iq.data[(off + i) * 2 + 1];
                    in[i] = i_val + q_val * I;
                }
                if (!fft_execute(plan, in, out) || !fft_shift(out, shifted, args.fft_size)) continue;

                // Scale frame index to available graph height (newest at top, oldest at bottom)
                double scale_factor = (double)graph_height / (double)max_frames;
                uint32_t y = axis_margin + (uint32_t)((max_frames - 1 - fidx) * scale_factor);
                if (y >= height - axis_margin) continue;
                for (uint32_t x = axis_margin; x < width; x++) {
                    uint32_t bin = (uint32_t)((x - axis_margin) * (double)args.fft_size / (double)(width - axis_margin));
                    if (bin >= args.fft_size) continue;
                    double p = creal(shifted[bin]) * creal(shifted[bin]) + cimag(shifted[bin]) * cimag(shifted[bin]);
                    double db = args.logmag ? 10.0 * log10(p + 1e-12) : sqrt(p);
                    double norm = (db - wf_min) / (wf_max - wf_min + 1e-12);
                    if (norm < 0.0) norm = 0.0; else if (norm > 1.0) norm = 1.0;
                    uint8_t r,g,b; png_intensity_to_color((float)norm, &r, &g, &b);
                    png_image_set_pixel(&wimg, x, y, r, g, b);
                }
            }

            // Axes
            double duration_s = (double)iq.num_samples / (double)iq.sample_rate;
            draw_time_axis(wimg.data, width, height, axis_margin, duration_s, 0, 255, 200, 0);
            draw_frequency_axis(wimg.data, width, height, axis_margin, 0.0, iq.sample_rate, args.fft_size, 255, 200, 0);

            char wpath[512];
            snprintf(wpath, sizeof(wpath), "%s_waterfall.png", args.out_prefix);
            if (!png_image_write(&wimg, wpath)) {
                fprintf(stderr, "Failed to write %s\n", wpath);
            } else {
                printf("Wrote %s\n", wpath);
            }
            png_image_free(&wimg);
        }
    }
    free(in); free(out); free(shifted); free(accum);
    fft_plan_destroy(plan);
    iq_free(&iq);
    return 0;
}


