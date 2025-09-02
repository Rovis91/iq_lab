#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include "../src/iq_core/io_iq.h"

// Command line arguments structure
typedef struct {
    const char *input_file;
    const char *format_str;
    uint32_t sample_rate;
    const char *meta_file;
    bool verbose;
} iqinfo_args_t;

/*
 * Parse command line arguments
 * Returns true on success, false on error
 */
bool parse_args(int argc, char *argv[], iqinfo_args_t *args) {
    // Initialize with defaults
    memset(args, 0, sizeof(iqinfo_args_t));
    args->sample_rate = 1000000;  // Default 1 Msps

    // Define long options for getopt
    static struct option long_options[] = {
        {"in", required_argument, 0, 'i'},
        {"format", required_argument, 0, 'f'},
        {"rate", required_argument, 0, 'r'},
        {"meta", required_argument, 0, 'm'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "i:f:r:m:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i':
                args->input_file = optarg;
                break;
            case 'f':
                args->format_str = optarg;
                break;
            case 'r':
                args->sample_rate = (uint32_t)atoi(optarg);
                break;
            case 'm':
                args->meta_file = optarg;
                break;
            case 'v':
                args->verbose = true;
                break;
            case 'h':
            default:
                fprintf(stderr, "Usage: %s --in <file.iq> --format {s8|s16} [--rate <Hz>] [--meta <file.sigmf-meta>]\n", argv[0]);
                fprintf(stderr, "Example: %s --in capture.iq --format s16 --rate 2000000\n", argv[0]);
                return false;
        }
    }

    // Validate required arguments
    if (!args->input_file) {
        fprintf(stderr, "Error: Input file is required (--in)\n");
        return false;
    }

    if (!args->format_str) {
        fprintf(stderr, "Error: Format is required (--format)\n");
        return false;
    }

    if (strcmp(args->format_str, "s8") != 0 && strcmp(args->format_str, "s16") != 0) {
        fprintf(stderr, "Error: Format must be 's8' or 's16'\n");
        return false;
    }

    return true;
}

/*
 * Calculate RMS (Root Mean Square) of IQ data
 * RMS represents the average power of the signal
 */
double calculate_rms(const float *data, size_t num_samples) {
    if (!data || num_samples == 0) {
        return 0.0;
    }

    double sum_squares = 0.0;

    // Sum of squares for both I and Q components
    for (size_t i = 0; i < num_samples; i++) {
        double i_val = (double)data[i * 2];     // I component
        double q_val = (double)data[i * 2 + 1]; // Q component
        sum_squares += (i_val * i_val) + (q_val * q_val);
    }

    // RMS = sqrt(average power)
    // Total samples = num_samples * 2 (I + Q per sample)
    return sqrt(sum_squares / (double)(num_samples * 2));
}

/*
 * Calculate DC offset (average value) for I and Q components
 */
void calculate_dc_offset(const float *data, size_t num_samples, double *dc_i, double *dc_q) {
    if (!data || num_samples == 0 || !dc_i || !dc_q) {
        *dc_i = 0.0;
        *dc_q = 0.0;
        return;
    }

    double sum_i = 0.0;
    double sum_q = 0.0;

    for (size_t i = 0; i < num_samples; i++) {
        sum_i += (double)data[i * 2];     // I component
        sum_q += (double)data[i * 2 + 1]; // Q component
    }

    *dc_i = sum_i / (double)num_samples;
    *dc_q = sum_q / (double)num_samples;
}

/*
 * Convert RMS to dBFS (decibels relative to full scale)
 * Full scale for float [-1,1] is 1.0
 */
double rms_to_dbfs(double rms) {
    if (rms <= 0.0) {
        return -INFINITY;  // Negative infinity for zero signal
    }
    return 20.0 * log10(rms);
}

/*
 * Main processing function
 * Loads IQ file and computes statistics
 */
int process_iq_file(const iqinfo_args_t *args) {
    iq_data_t iq_data = {0};

    // Determine format
    iq_format_t format = IQ_FORMAT_S8;
    if (strcmp(args->format_str, "s16") == 0) {
        format = IQ_FORMAT_S16;
    }

    if (args->verbose) {
        printf("Loading IQ file: %s (format: %s, rate: %u Hz)\n",
               args->input_file, args->format_str, args->sample_rate);
    }

    // Load IQ file
    if (!iq_load_file(args->input_file, &iq_data)) {
        fprintf(stderr, "Error: Failed to load IQ file '%s'\n", args->input_file);
        return EXIT_FAILURE;
    }

    // Set sample rate (could be overridden by SigMF metadata later)
    iq_data.sample_rate = args->sample_rate;

    if (args->verbose) {
        printf("Loaded %zu complex samples (%.3f seconds at %u Hz)\n",
               iq_data.num_samples,
               (double)iq_data.num_samples / (double)args->sample_rate,
               args->sample_rate);
    }

    // Calculate statistics
    double rms = calculate_rms(iq_data.data, iq_data.num_samples);
    double rms_dbfs = rms_to_dbfs(rms);

    double dc_i, dc_q;
    calculate_dc_offset(iq_data.data, iq_data.num_samples, &dc_i, &dc_q);

    double duration_s = (double)iq_data.num_samples / (double)args->sample_rate;

    // Output JSON result
    printf("{\n");
    printf("  \"input_file\": \"%s\",\n", args->input_file);
    printf("  \"format\": \"%s\",\n", args->format_str);
    printf("  \"sample_rate\": %u,\n", args->sample_rate);
    printf("  \"duration_s\": %.6f,\n", duration_s);
    printf("  \"num_samples\": %zu,\n", iq_data.num_samples);
    printf("  \"rms_dBFS\": %.2f,\n", rms_dbfs);
    printf("  \"dc_offset_I\": %.6f,\n", dc_i);
    printf("  \"dc_offset_Q\": %.6f,\n", dc_q);
    printf("  \"estimated_noise_floor_dBFS\": %.2f\n", rms_dbfs - 10.0); // Rough estimate
    printf("}\n");

    // Cleanup
    iq_free(&iq_data);

    return EXIT_SUCCESS;
}

/*
 * Main entry point
 */
int main(int argc, char *argv[]) {
    iqinfo_args_t args;

    // Parse command line arguments
    if (!parse_args(argc, argv, &args)) {
        return EXIT_FAILURE;
    }

    // Process the IQ file
    return process_iq_file(&args);
}
