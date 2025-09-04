/*
 * IQ Lab - iqdetect: Signal Detection and Classification Tool
 *
 * Purpose: Complete signal detection pipeline with CFAR, clustering, and feature extraction
  *
 *
 * This tool implements a comprehensive signal detection and classification pipeline
 * that processes IQ data to identify and characterize signals in the frequency domain.
 * It combines OS-CFAR detection, temporal clustering, and feature extraction to
 * produce structured event outputs with SNR, bandwidth, and modulation information.
 *
 * Key Features:
 * - FFT-based signal detection using OS-CFAR algorithm
 * - Temporal clustering with hysteresis for event formation
 * - Feature extraction including SNR, bandwidth, and modulation classification
 * - Structured event output in CSV or JSONL format
 * - Optional IQ cutouts for detected signals
 * - Configurable detection parameters for different scenarios
 *
 * Usage Examples:
 *   # Basic signal detection
 *   iqdetect.exe --in signal.iq --rate 2000000 --fft 4096 --hop 1024 --pfa 1e-3 --out events.csv
 *
 *   # Advanced detection with cutouts
 *   iqdetect.exe --in signal.iq --rate 2000000 --fft 4096 --hop 1024 --pfa 1e-3 --cut --out events.csv
 *
 *   # JSON output with custom parameters
 *   iqdetect.exe --in signal.iq --format jsonl --max-time-gap 100 --max-freq-gap 20000 --out events.jsonl
 *
 * Technical Pipeline:
 * 1. Load IQ data with SigMF metadata
 * 2. Streaming FFT processing with configurable hop size
 * 3. OS-CFAR detection on each FFT frame
 * 4. Temporal clustering of detections
 * 5. Feature extraction for each event
 * 6. Event output with structured format
 * 7. Optional IQ cutout generation
 *
 * Output Formats:
 * - CSV: Standard spreadsheet format with headers
 * - JSONL: One JSON object per line for streaming processing
 * - IQ Cutouts: Individual IQ files for each detected event
 *
 * Performance:
 * - Real-time capable with proper parameter tuning
 * - Memory efficient streaming processing
 * - Configurable for different signal densities
 *
 * Applications:
 * - RF signal monitoring and surveillance
 * - Spectrum occupancy analysis
 * - Interference detection and classification
 * - Communications signal analysis
 * - Automatic signal discovery and characterization
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#include "../src/iq_core/io_iq.h"
#include "../src/iq_core/io_sigmf.h"
#include "../src/iq_core/fft.h"
#include "../src/detect/cfar_os.h"
#include "../src/detect/cluster.h"
#include "../src/detect/features.h"

// Configuration structure for iqdetect
typedef struct {
    // Input parameters
    const char *input_file;
    const char *meta_file;
    const char *format_str;     // s8|s16
    uint32_t sample_rate;

    // Detection parameters
    uint32_t fft_size;
    uint32_t hop_size;
    double pfa;                // Probability of False Alarm
    uint32_t ref_cells;        // CFAR reference cells
    uint32_t guard_cells;      // CFAR guard cells
    uint32_t os_rank;          // OS-CFAR rank parameter

    // Clustering parameters
    double max_time_gap_ms;    // Maximum time gap for clustering (milliseconds)
    double max_freq_gap_hz;    // Maximum frequency gap for clustering (Hz)
    uint32_t max_clusters;     // Maximum number of active clusters

    // Output parameters
    const char *output_file;
    const char *output_format; // csv|jsonl
    bool generate_cutouts;     // Whether to generate IQ cutouts

    // Processing options
    bool verbose;              // Verbose output
} iqdetect_config_t;

// Detection context structure
typedef struct {
    // File I/O
    iq_data_t iq_data;
    sigmf_metadata_t sigmf_meta;

    // Processing modules
    fft_plan_t *fft_plan;
    cfar_os_t cfar_detector;
    cluster_t cluster_engine;
    features_t feature_extractor;

    // FFT working buffers
    fft_complex_t *fft_in;
    fft_complex_t *fft_out;
    fft_complex_t *fft_shifted;
    double *power_spectrum;

    // Configuration
    iqdetect_config_t *config;
} iqdetect_context_t;

// Function prototypes
static void print_usage(void);
static bool parse_arguments(int argc, char **argv, iqdetect_config_t *config);
static bool initialize_context(iqdetect_context_t *ctx, iqdetect_config_t *config);
static void cleanup_context(iqdetect_context_t *ctx);
static bool process_iq_data(iqdetect_context_t *ctx);
static bool write_events_csv(const cluster_event_t *events, uint32_t num_events,
                           iqdetect_context_t *ctx);
static bool write_events_jsonl(const cluster_event_t *events, uint32_t num_events,
                             iqdetect_context_t *ctx);
static bool generate_iq_cutout(const cluster_event_t *event, uint32_t event_index,
                             iqdetect_context_t *ctx);

// Main entry point
int main(int argc, char **argv) {
    iqdetect_config_t config = {
        .fft_size = 4096,
        .hop_size = 1024,
        .pfa = 1e-3,
        .ref_cells = 16,
        .guard_cells = 2,
        .os_rank = 8,
        .max_time_gap_ms = 50.0,
        .max_freq_gap_hz = 10000.0,
        .max_clusters = 100,
        .output_format = "csv",
        .generate_cutouts = false,
        .verbose = false
    };

    if (!parse_arguments(argc, argv, &config)) {
        print_usage();
        return 1;
    }

    // Initialize processing context
    iqdetect_context_t context;
    memset(&context, 0, sizeof(context));
    context.config = &config;

    if (!initialize_context(&context, &config)) {
        fprintf(stderr, "Failed to initialize detection context\n");
        cleanup_context(&context);
        return 1;
    }

    if (config.verbose) {
        printf("IQ Detect Configuration:\n");
        printf("  Input: %s\n", config.input_file);
        printf("  Sample Rate: %u Hz\n", config.sample_rate);
        printf("  FFT Size: %u, Hop Size: %u\n", config.fft_size, config.hop_size);
        printf("  PFA: %.2e, Ref Cells: %u, Guard Cells: %u, OS Rank: %u\n",
               config.pfa, config.ref_cells, config.guard_cells, config.os_rank);
        printf("  Max Time Gap: %.1f ms, Max Freq Gap: %.0f Hz\n",
               config.max_time_gap_ms, config.max_freq_gap_hz);
        printf("  Output: %s (%s)\n", config.output_file, config.output_format);
        printf("\n");
    }

    // Process the IQ data
    bool success = process_iq_data(&context);

    // Cleanup
    cleanup_context(&context);

    if (success) {
        if (config.verbose) {
            printf("Signal detection completed successfully\n");
        }
        return 0;
    } else {
        fprintf(stderr, "Signal detection failed\n");
        return 1;
    }
}

// Print usage information
static void print_usage(void) {
    printf("IQ Lab - iqdetect: Signal Detection and Classification Tool\n\n");
    printf("Usage: iqdetect --in <file> [--meta <meta>] --format {s8|s16} --rate <Hz> [options] --out <file>\n\n");
    printf("Required Arguments:\n");
    printf("  --in <file>          Input IQ file\n");
    printf("  --format {s8|s16}    IQ data format\n");
    printf("  --rate <Hz>          Sample rate\n");
    printf("  --out <file>         Output events file\n\n");
    printf("Detection Parameters:\n");
    printf("  --fft <N>            FFT size (default: 4096)\n");
    printf("  --hop <N>            FFT hop size (default: 1024)\n");
    printf("  --pfa <float>        Probability of False Alarm (default: 1e-3)\n");
    printf("  --ref-cells <N>      CFAR reference cells (default: 16)\n");
    printf("  --guard-cells <N>    CFAR guard cells (default: 2)\n");
    printf("  --os-rank <N>        OS-CFAR rank parameter (default: 8)\n\n");
    printf("Clustering Parameters:\n");
    printf("  --max-time-gap <ms>  Maximum time gap for clustering (default: 50.0)\n");
    printf("  --max-freq-gap <Hz>  Maximum frequency gap for clustering (default: 10000.0)\n");
    printf("  --max-clusters <N>   Maximum number of active clusters (default: 100)\n\n");
    printf("Output Options:\n");
    printf("  --format {csv|jsonl} Output format (default: csv)\n");
    printf("  --cut                Generate IQ cutouts for detected events\n");
    printf("  --verbose            Enable verbose output\n\n");
    printf("Examples:\n");
    printf("  iqdetect --in signal.iq --format s16 --rate 2000000 --out events.csv\n");
    printf("  iqdetect --in signal.iq --format s16 --rate 2000000 --pfa 1e-4 --cut --out events.jsonl\n");
}

// Parse command line arguments
static bool parse_arguments(int argc, char **argv, iqdetect_config_t *config) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--in") == 0 && i + 1 < argc) {
            config->input_file = argv[++i];
        } else if (strcmp(argv[i], "--meta") == 0 && i + 1 < argc) {
            config->meta_file = argv[++i];
        } else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            config->format_str = argv[++i];
        } else if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            config->sample_rate = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--fft") == 0 && i + 1 < argc) {
            config->fft_size = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--hop") == 0 && i + 1 < argc) {
            config->hop_size = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--pfa") == 0 && i + 1 < argc) {
            config->pfa = atof(argv[++i]);
        } else if (strcmp(argv[i], "--ref-cells") == 0 && i + 1 < argc) {
            config->ref_cells = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--guard-cells") == 0 && i + 1 < argc) {
            config->guard_cells = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--os-rank") == 0 && i + 1 < argc) {
            config->os_rank = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--max-time-gap") == 0 && i + 1 < argc) {
            config->max_time_gap_ms = atof(argv[++i]);
        } else if (strcmp(argv[i], "--max-freq-gap") == 0 && i + 1 < argc) {
            config->max_freq_gap_hz = atof(argv[++i]);
        } else if (strcmp(argv[i], "--max-clusters") == 0 && i + 1 < argc) {
            config->max_clusters = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            config->output_file = argv[++i];
        } else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            config->output_format = argv[++i];
        } else if (strcmp(argv[i], "--cut") == 0) {
            config->generate_cutouts = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            config->verbose = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            exit(0);
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return false;
        }
    }

    // Validate required parameters
    if (!config->input_file || !config->format_str || !config->sample_rate || !config->output_file) {
        fprintf(stderr, "Missing required arguments\n");
        return false;
    }

    // Validate parameter ranges
    if (!cfar_os_validate_params(config->fft_size, config->pfa, config->ref_cells,
                               config->guard_cells, config->os_rank)) {
        fprintf(stderr, "Invalid CFAR parameters\n");
        return false;
    }

    if (!cluster_validate_params(config->max_time_gap_ms, config->max_freq_gap_hz,
                               config->max_clusters, (double)config->sample_rate)) {
        fprintf(stderr, "Invalid clustering parameters\n");
        return false;
    }

    return true;
}

// Initialize detection context
static bool initialize_context(iqdetect_context_t *ctx, iqdetect_config_t *config) {
    memset(ctx, 0, sizeof(*ctx));

    // Load IQ data
    if (!iq_load_file(config->input_file, &ctx->iq_data)) {
        fprintf(stderr, "Failed to load IQ file: %s\n", config->input_file);
        return false;
    }

    if (config->sample_rate) {
        ctx->iq_data.sample_rate = config->sample_rate;
    }

    if (ctx->iq_data.sample_rate == 0) {
        fprintf(stderr, "Invalid sample rate\n");
        return false;
    }

    // Load SigMF metadata if provided
    if (config->meta_file) {
        if (!sigmf_read_metadata(config->meta_file, &ctx->sigmf_meta)) {
            fprintf(stderr, "Failed to load SigMF metadata: %s\n", config->meta_file);
            // Continue without metadata - not a fatal error
        }
    }

    // Initialize FFT
    ctx->fft_plan = fft_plan_create(config->fft_size, FFT_FORWARD);
    if (!ctx->fft_plan) {
        fprintf(stderr, "Failed to create FFT plan\n");
        return false;
    }

    // Allocate FFT buffers
    ctx->fft_in = malloc(config->fft_size * sizeof(fft_complex_t));
    ctx->fft_out = malloc(config->fft_size * sizeof(fft_complex_t));
    ctx->fft_shifted = malloc(config->fft_size * sizeof(fft_complex_t));
    ctx->power_spectrum = malloc(config->fft_size * sizeof(double));

    if (!ctx->fft_in || !ctx->fft_out || !ctx->fft_shifted || !ctx->power_spectrum) {
        fprintf(stderr, "Failed to allocate FFT buffers\n");
        return false;
    }

    // Initialize CFAR detector
    if (!cfar_os_init(&ctx->cfar_detector, config->fft_size, config->pfa,
                     config->ref_cells, config->guard_cells, config->os_rank)) {
        fprintf(stderr, "Failed to initialize CFAR detector\n");
        return false;
    }

    // Initialize clustering
    if (!cluster_init(&ctx->cluster_engine, config->max_time_gap_ms, config->max_freq_gap_hz,
                     config->max_clusters, (double)config->sample_rate)) {
        fprintf(stderr, "Failed to initialize clustering\n");
        return false;
    }

    // Initialize feature extraction
    if (!features_init(&ctx->feature_extractor, config->fft_size, (double)config->sample_rate, 128)) {
        fprintf(stderr, "Failed to initialize feature extraction\n");
        return false;
    }

    return true;
}

// Clean up detection context
static void cleanup_context(iqdetect_context_t *ctx) {
    // Free FFT resources
    if (ctx->fft_plan) {
        fft_plan_destroy(ctx->fft_plan);
    }
    free(ctx->fft_in);
    free(ctx->fft_out);
    free(ctx->fft_shifted);
    free(ctx->power_spectrum);

    // Clean up detection modules
    cfar_os_free(&ctx->cfar_detector);
    cluster_free(&ctx->cluster_engine);
    features_free(&ctx->feature_extractor);

    // Clean up data
    iq_free(&ctx->iq_data);
    sigmf_free_metadata(&ctx->sigmf_meta);
}

// Process IQ data through detection pipeline
static bool process_iq_data(iqdetect_context_t *ctx) {
    iqdetect_config_t *config = ctx->config;

    if (config->verbose) {
        printf("Processing %zu IQ samples at %u Hz...\n",
               ctx->iq_data.num_samples, ctx->iq_data.sample_rate);
    }

    uint64_t num_frames = 0;
    uint64_t total_detections = 0;

    // Process data in overlapping frames
    for (uint64_t offset = 0; offset + config->fft_size <= ctx->iq_data.num_samples;
         offset += config->hop_size) {

        // Prepare FFT input
        for (uint32_t i = 0; i < config->fft_size; i++) {
            float i_val = ctx->iq_data.data[(offset + i) * 2];
            float q_val = ctx->iq_data.data[(offset + i) * 2 + 1];
            ctx->fft_in[i] = i_val + q_val * I;
        }

        // Execute FFT
        if (!fft_execute(ctx->fft_plan, ctx->fft_in, ctx->fft_out)) {
            fprintf(stderr, "FFT execution failed at frame %llu\n", (unsigned long long)num_frames);
            continue;
        }

        // FFT shift
        if (!fft_shift(ctx->fft_out, ctx->fft_shifted, config->fft_size)) {
            fprintf(stderr, "FFT shift failed at frame %llu\n", (unsigned long long)num_frames);
            continue;
        }

        // Calculate power spectrum
        for (uint32_t i = 0; i < config->fft_size; i++) {
            double real = creal(ctx->fft_shifted[i]);
            double imag = cimag(ctx->fft_shifted[i]);
            ctx->power_spectrum[i] = real * real + imag * imag;
        }

        // Detect signals
        cfar_detection_t detections[100]; // Reasonable maximum
        uint32_t num_detections = cfar_os_process_frame(&ctx->cfar_detector,
                                                      ctx->power_spectrum,
                                                      detections, 100);

        total_detections += num_detections;

        // Add detections to clustering
        double frame_time = (double)offset / (double)config->sample_rate;
        for (uint32_t i = 0; i < num_detections; i++) {
            if (!cluster_add_detection(&ctx->cluster_engine, &detections[i], frame_time)) {
                if (config->verbose) {
                    printf("Warning: Failed to add detection to cluster\n");
                }
            }
        }

        // Extract completed events periodically
        cluster_event_t events[50];
        uint32_t num_events = cluster_get_events(&ctx->cluster_engine, events, 50, frame_time);

        if (num_events > 0) {
            if (config->verbose) {
                printf("Extracted %u events at frame %llu\n", num_events, (unsigned long long)num_frames);
            }

            // Write events to output file
            if (strcmp(config->output_format, "jsonl") == 0) {
                write_events_jsonl(events, num_events, ctx);
            } else {
                write_events_csv(events, num_events, ctx);
            }

                    // Generate cutouts if requested
        if (config->generate_cutouts) {
            for (uint32_t i = 0; i < num_events; i++) {
                generate_iq_cutout(&events[i], i, ctx);
            }
        }
        }

        num_frames++;
    }

    // Extract any remaining events
    cluster_event_t events[50];
    double final_time = (double)ctx->iq_data.num_samples / (double)config->sample_rate;
    uint32_t num_events = cluster_get_events(&ctx->cluster_engine, events, 50, final_time);

    if (num_events > 0) {
        if (config->verbose) {
            printf("Extracted %u final events\n", num_events);
        }

        if (strcmp(config->output_format, "jsonl") == 0) {
            write_events_jsonl(events, num_events, ctx);
        } else {
            write_events_csv(events, num_events, ctx);
        }

        if (config->generate_cutouts) {
            for (uint32_t i = 0; i < num_events; i++) {
                generate_iq_cutout(&events[i], i, ctx);
            }
        }
    }

    if (config->verbose) {
        printf("Processing complete:\n");
        printf("  Frames processed: %llu\n", (unsigned long long)num_frames);
        printf("  Total detections: %llu\n", (unsigned long long)total_detections);
        printf("  Events extracted: %u\n", cluster_get_active_count(&ctx->cluster_engine));
    }

    return true;
}

// Write events in CSV format
static bool write_events_csv(const cluster_event_t *events, uint32_t num_events,
                           iqdetect_context_t *ctx) {
    static bool header_written = false;
    iqdetect_config_t *config = ctx->config;

    FILE *file = fopen(config->output_file, header_written ? "a" : "w");
    if (!file) {
        fprintf(stderr, "Failed to open output file: %s\n", config->output_file);
        return false;
    }

    // Write header if this is the first write
    if (!header_written) {
        fprintf(file, "t_start_s,t_end_s,f_center_Hz,bw_Hz,snr_dB,peak_dBFS,modulation_guess,confidence_0_1,tags\n");
        header_written = true;
    }

    // Write events
    for (uint32_t i = 0; i < num_events; i++) {
        const cluster_event_t *event = &events[i];
        fprintf(file, "%.6f,%.6f,%.3f,%.3f,%.2f,%.2f,%s,%.3f,\"burst,detection\"\n",
                event->start_time_s, event->end_time_s,
                event->center_freq_hz, event->bandwidth_hz,
                event->peak_snr_db, event->peak_power_dbfs,
                event->modulation_guess ? event->modulation_guess : "unknown",
                event->confidence);
    }

    fclose(file);
    return true;
}

// Write events in JSONL format
static bool write_events_jsonl(const cluster_event_t *events, uint32_t num_events,
                             iqdetect_context_t *ctx) {
    iqdetect_config_t *config = ctx->config;

    FILE *file = fopen(config->output_file, "a");
    if (!file) {
        fprintf(stderr, "Failed to open output file: %s\n", config->output_file);
        return false;
    }

    // Write events as JSON objects
    for (uint32_t i = 0; i < num_events; i++) {
        const cluster_event_t *event = &events[i];
        fprintf(file, "{\"t_start_s\":%.6f,\"t_end_s\":%.6f,\"f_center_Hz\":%.3f,\"bw_Hz\":%.3f,"
                     "\"snr_dB\":%.2f,\"peak_dBFS\":%.2f,\"modulation_guess\":\"%s\",\"confidence_0_1\":%.3f,"
                     "\"tags\":[\"burst\",\"detection\"]}\n",
                event->start_time_s, event->end_time_s,
                event->center_freq_hz, event->bandwidth_hz,
                event->peak_snr_db, event->peak_power_dbfs,
                event->modulation_guess ? event->modulation_guess : "unknown",
                event->confidence);
    }

    fclose(file);
    return true;
}

// Generate IQ cutout for detected event
static bool generate_iq_cutout(const cluster_event_t *event, uint32_t event_index,
                             iqdetect_context_t *ctx) {
    iqdetect_config_t *config = ctx->config;

    // Calculate time bounds for cutout (add some padding around the event)
    double padding_seconds = 0.001; // 1ms padding
    uint64_t start_sample = (uint64_t)((event->start_time_s - padding_seconds) * config->sample_rate);
    uint64_t end_sample = (uint64_t)((event->end_time_s + padding_seconds) * config->sample_rate);

    // Ensure bounds are within the original file
    if (start_sample >= ctx->iq_data.num_samples) {
        return false; // Event starts after end of file
    }
    if (end_sample >= ctx->iq_data.num_samples) {
        end_sample = ctx->iq_data.num_samples - 1;
    }
    if (start_sample >= end_sample) {
        start_sample = 0; // Start from beginning if padding goes negative
    }

    // Create output filename
    char iq_filename[512];
    char meta_filename[512];
    snprintf(iq_filename, sizeof(iq_filename), "event_%03u.iq", event_index);
    snprintf(meta_filename, sizeof(meta_filename), "event_%03u.sigmf-meta", event_index);

    // Create new IQ data structure for cutout
    iq_data_t cutout = {0};
    cutout.sample_rate = ctx->iq_data.sample_rate;
    cutout.format = ctx->iq_data.format; // Use same format as input
    cutout.num_samples = end_sample - start_sample + 1;
    cutout.data = malloc(cutout.num_samples * 2 * sizeof(float));

    if (!cutout.data) {
        return false;
    }

    // Copy IQ samples with padding
    for (uint64_t i = start_sample; i <= end_sample; i++) {
        uint64_t src_idx = i * 2;
        uint64_t dst_idx = (i - start_sample) * 2;
        cutout.data[dst_idx] = ctx->iq_data.data[src_idx];         // I
        cutout.data[dst_idx + 1] = ctx->iq_data.data[src_idx + 1]; // Q
    }

    // Save IQ cutout
    if (!iq_data_save_file(iq_filename, &cutout)) {
        free(cutout.data);
        return false;
    }

    free(cutout.data);

    // Create SigMF metadata for cutout
    sigmf_metadata_t cutout_meta = {0};
    sigmf_create_basic_metadata(&cutout_meta, "cf32", cutout.sample_rate, 0,
                               "IQ cutout from detected event", "iqdetect");

    // Add capture information with timing offset
    sigmf_add_capture(&cutout_meta, (uint64_t)((event->start_time_s - padding_seconds) * cutout.sample_rate), 0, NULL);

    // Add annotation for the detected event
    char annotation[256];
    snprintf(annotation, sizeof(annotation),
             "Detected event: SNR=%.1f dB, BW=%.0f Hz, modulation=%s, confidence=%.2f",
             event->peak_snr_db, event->bandwidth_hz,
             event->modulation_guess ? event->modulation_guess : "unknown",
             event->confidence);

    // Annotation spans the actual event (without padding)
    uint64_t event_start_in_cutout = (uint64_t)(padding_seconds * cutout.sample_rate);
    uint64_t event_end_in_cutout = event_start_in_cutout + (uint64_t)((event->end_time_s - event->start_time_s) * cutout.sample_rate);

    sigmf_add_annotation(&cutout_meta, event_start_in_cutout, event_end_in_cutout,
                        event->center_freq_hz - event->bandwidth_hz/2,
                        event->center_freq_hz + event->bandwidth_hz/2,
                        annotation);

    // Save metadata
    if (!sigmf_write_metadata(meta_filename, &cutout_meta)) {
        sigmf_free_metadata(&cutout_meta);
        return false;
    }

    sigmf_free_metadata(&cutout_meta);

    if (config->verbose) {
        printf("Generated cutout: %s (%llu samples, %.3f s duration)\n",
               iq_filename, (unsigned long long)cutout.num_samples,
               (double)cutout.num_samples / (double)cutout.sample_rate);
    }

    return true;
}
