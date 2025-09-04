/*
 * IQ Lab - iqchan: Polyphase Filter Bank Channelizer
 *
 * Purpose: Channelize wideband IQ signals into multiple narrow-band channels
 * using efficient polyphase filter bank techniques for signal analysis.
 *
 * Usage: iqchan --in <input.iq> --format {s8|s16} --rate <sample_rate> \
 *               --channels <N> --bandwidth <Hz> --out <output_dir>
 *
 *
 * Date: 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "../src/iq_core/io_iq.h"
#include "../src/iq_core/io_sigmf.h"
#include "../src/chan/pfb.h"

/**
 * @brief Command-line options structure
 */
typedef struct {
    const char *input_file;
    const char *output_dir;
    const char *format;
    const char *meta_file;
    double sample_rate;
    uint32_t num_channels;
    double channel_bandwidth;
    double overlap;
    uint32_t fft_size;
    bool verbose;
    bool help;
} iqchan_options_t;

/**
 * @brief Default options
 */
static const iqchan_options_t DEFAULT_OPTIONS = {
    .input_file = NULL,
    .output_dir = "channels",
    .format = "s16",
    .meta_file = NULL,
    .sample_rate = 2000000.0,
    .num_channels = 8,
    .channel_bandwidth = 250000.0,
    .overlap = 0.1,
    .fft_size = 4096,
    .verbose = false,
    .help = false
};

/**
 * @brief Print usage information
 */
static void print_usage(const char *program_name) {
    printf("IQ Lab - iqchan: Polyphase Filter Bank Channelizer\n\n");
    printf("USAGE:\n");
    printf("  %s --in <input.iq> --format {s8|s16} --rate <sample_rate> \\\n", program_name);
    printf("       --channels <N> --bandwidth <Hz> --out <output_dir> [options]\n\n");

    printf("REQUIRED ARGUMENTS:\n");
    printf("  --in <file>           Input IQ file path\n");
    printf("  --format {s8|s16}     IQ data format\n");
    printf("  --rate <Hz>           Sample rate in Hz\n");
    printf("  --channels <N>        Number of output channels (4-32)\n");
    printf("  --bandwidth <Hz>      Bandwidth per channel in Hz\n");
    printf("  --out <dir>           Output directory for channel files\n\n");

    printf("OPTIONAL ARGUMENTS:\n");
    printf("  --meta <file>         SigMF metadata file (auto-detected if not specified)\n");
    printf("  --overlap <ratio>     Channel overlap ratio (0.0-0.5, default: 0.1)\n");
    printf("  --fft <size>          FFT size (power of 2, default: 4096)\n");
    printf("  --verbose             Enable verbose output\n");
    printf("  --help                Show this help message\n\n");

    printf("EXAMPLES:\n");
    printf("  # Basic channelization of 2MHz signal into 8 channels\n");
    printf("  %s --in capture.iq --format s16 --rate 2000000 \\\n", program_name);
    printf("       --channels 8 --bandwidth 250000 --out channels/\n\n");

    printf("  # High-resolution channelization with overlap\n");
    printf("  %s --in signal.iq --format s16 --rate 10000000 \\\n", program_name);
    printf("       --channels 16 --bandwidth 500000 --overlap 0.2 \\\n");
    printf("       --fft 8192 --out high_res_channels/ --verbose\n\n");

    printf("OUTPUT:\n");
    printf("  Creates channel_00.iq, channel_00.sigmf-meta,\n");
    printf("  channel_01.iq, channel_01.sigmf-meta, etc.\n\n");
}

/**
 * @brief Parse command-line arguments
 */
static bool parse_arguments(int argc, char **argv, iqchan_options_t *options) {
    *options = DEFAULT_OPTIONS;

    static struct option long_options[] = {
        {"in", required_argument, 0, 'i'},
        {"out", required_argument, 0, 'o'},
        {"format", required_argument, 0, 'f'},
        {"meta", required_argument, 0, 'm'},
        {"rate", required_argument, 0, 'r'},
        {"channels", required_argument, 0, 'c'},
        {"bandwidth", required_argument, 0, 'b'},
        {"overlap", required_argument, 0, 'l'},
        {"fft", required_argument, 0, 'F'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "i:o:f:m:r:c:b:l:F:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i':
                options->input_file = optarg;
                break;
            case 'o':
                options->output_dir = optarg;
                break;
            case 'f':
                options->format = optarg;
                break;
            case 'm':
                options->meta_file = optarg;
                break;
            case 'r':
                options->sample_rate = atof(optarg);
                break;
            case 'c':
                options->num_channels = (uint32_t)atoi(optarg);
                break;
            case 'b':
                options->channel_bandwidth = atof(optarg);
                break;
            case 'l':
                options->overlap = atof(optarg);
                break;
            case 'F':
                options->fft_size = (uint32_t)atoi(optarg);
                break;
            case 'v':
                options->verbose = true;
                break;
            case 'h':
                options->help = true;
                return true;
            default:
                fprintf(stderr, "Unknown option. Use --help for usage.\n");
                return false;
        }
    }

    return true;
}

/**
 * @brief Validate command-line options
 */
static bool validate_options(const iqchan_options_t *options) {
    if (!options->input_file) {
        fprintf(stderr, "ERROR: Input file not specified. Use --in <file>\n");
        return false;
    }

    if (access(options->input_file, F_OK) != 0) {
        fprintf(stderr, "ERROR: Input file '%s' does not exist\n", options->input_file);
        return false;
    }

    if (options->num_channels < 4 || options->num_channels > 32) {
        fprintf(stderr, "ERROR: Number of channels must be between 4 and 32\n");
        return false;
    }

    if (options->sample_rate <= 0.0) {
        fprintf(stderr, "ERROR: Sample rate must be positive\n");
        return false;
    }

    if (options->channel_bandwidth <= 0.0) {
        fprintf(stderr, "ERROR: Channel bandwidth must be positive\n");
        return false;
    }

    if (options->overlap < 0.0 || options->overlap >= 1.0) {
        fprintf(stderr, "ERROR: Channel overlap must be between 0.0 and 1.0\n");
        return false;
    }

    if (options->fft_size & (options->fft_size - 1)) {
        fprintf(stderr, "ERROR: FFT size must be a power of 2\n");
        return false;
    }

    if (strcmp(options->format, "s8") != 0 && strcmp(options->format, "s16") != 0) {
        fprintf(stderr, "ERROR: Format must be 's8' or 's16'\n");
        return false;
    }

    return true;
}

/**
 * @brief Create output directory
 */
static bool create_output_directory(const char *dir_path) {
    // Check if directory already exists
    struct stat st;
    if (stat(dir_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return true; // Directory already exists
        } else {
            fprintf(stderr, "ERROR: '%s' exists but is not a directory\n", dir_path);
            return false;
        }
    }

    // Create directory
#ifdef _WIN32
    if (mkdir(dir_path) != 0) {
#else
    if (mkdir(dir_path, 0755) != 0) {
#endif
        fprintf(stderr, "ERROR: Failed to create output directory '%s': %s\n",
                dir_path, strerror(errno));
        return false;
    }

    return true;
}

/**
 * @brief Generate channel metadata
 */
static bool generate_channel_metadata(const char *output_dir, uint32_t channel_index,
                                    const iqchan_options_t *options, pfb_t *pfb) {
    char meta_filename[512];
    snprintf(meta_filename, sizeof(meta_filename), "%s/channel_%02u.sigmf-meta",
             output_dir, channel_index);

    // Create metadata structure
    sigmf_metadata_t meta = {0};

    // Global metadata
    strcpy(meta.global.datatype, strcmp(options->format, "s8") == 0 ? "ci8" : "ci16");
    meta.global.sample_rate = (uint64_t)options->sample_rate;
    strcpy(meta.global.version, "1.2.0");
    snprintf(meta.global.description, sizeof(meta.global.description),
             "Channel %u extracted by iqchan from wideband signal", channel_index);

    // Captures metadata
    meta.num_captures = 1;
    meta.captures = (sigmf_capture_t *)malloc(sizeof(sigmf_capture_t));
    if (!meta.captures) return false;

    meta.captures[0].sample_start = 0;
    meta.captures[0].frequency = (uint64_t)pfb_get_channel_frequency(pfb, channel_index);

    // Annotations metadata
    meta.num_annotations = 1;
    meta.annotations = (sigmf_annotation_t *)malloc(sizeof(sigmf_annotation_t));
    if (!meta.annotations) {
        free(meta.captures);
        return false;
    }

    meta.annotations[0].sample_start = 0;
    meta.annotations[0].sample_count = 0; // Will be updated when writing
    meta.annotations[0].freq_lower_edge = (uint64_t)(pfb_get_channel_frequency(pfb, channel_index) -
                                                   pfb_get_channel_bandwidth(pfb, channel_index) / 2.0);
    meta.annotations[0].freq_upper_edge = (uint64_t)(pfb_get_channel_frequency(pfb, channel_index) +
                                                   pfb_get_channel_bandwidth(pfb, channel_index) / 2.0);
    snprintf(meta.annotations[0].description, sizeof(meta.annotations[0].description),
             "Channel %u: center=%.0f Hz, bandwidth=%.0f Hz",
             channel_index,
             pfb_get_channel_frequency(pfb, channel_index),
             pfb_get_channel_bandwidth(pfb, channel_index));

    // Write metadata
    bool success = sigmf_write_metadata(meta_filename, &meta);

    // Cleanup
    free(meta.annotations);
    free(meta.captures);

    return success;
}

/**
 * @brief Process IQ file through channelizer
 */
static bool process_file(const iqchan_options_t *options) {
    // Create output directory
    if (!create_output_directory(options->output_dir)) {
        return false;
    }

    if (options->verbose) {
        printf("📁 Created output directory: %s\n", options->output_dir);
    }

    // Load IQ data
    iq_data_t iq_data = {0};
    bool load_success = iq_load_file(options->input_file, &iq_data);

    if (!load_success) {
        fprintf(stderr, "ERROR: Failed to load IQ file '%s'\n", options->input_file);
        return false;
    }

    if (options->verbose) {
        printf("📡 Loaded IQ data: %llu samples, %.0f Hz sample rate\n",
               (unsigned long long)iq_data.num_samples, (double)iq_data.sample_rate);
    }

    // Override sample rate if specified
    iq_data.sample_rate = options->sample_rate;

    // Initialize PFB channelizer
    pfb_config_t pfb_config;
    if (!pfb_config_init(&pfb_config, options->num_channels,
                        options->sample_rate, options->channel_bandwidth)) {
        fprintf(stderr, "ERROR: Failed to initialize PFB configuration\n");
        iq_free(&iq_data);
        return false;
    }

    // Override FFT size and ensure filter_length is compatible
    pfb_config.fft_size = options->fft_size;
    // Adjust filter_length to be at least as large as FFT size and divisible by num_channels
    if (pfb_config.fft_size > pfb_config.filter_length) {
        pfb_config.filter_length = pfb_config.fft_size * pfb_config.num_channels;
        // Round up to next power of 2 for efficiency
        uint32_t power_of_2 = 1;
        while (power_of_2 < pfb_config.filter_length) {
            power_of_2 *= 2;
        }
        pfb_config.filter_length = power_of_2;
    }
    pfb_config.overlap_factor = options->overlap;

    if (options->verbose) {
        printf("🔧 PFB Configuration:\n");
        printf("   Channels: %u\n", pfb_config.num_channels);
        printf("   Sample Rate: %.0f Hz\n", pfb_config.sample_rate);
        printf("   Channel BW: %.0f Hz\n", pfb_config.channel_bandwidth);
        printf("   FFT Size: %u\n", pfb_config.fft_size);
        printf("   Overlap: %.2f\n", pfb_config.overlap_factor);
    }

    pfb_t *pfb = pfb_create(&pfb_config);
    if (!pfb) {
        fprintf(stderr, "ERROR: Failed to create PFB channelizer\n");
        fprintf(stderr, "  Debug info: channels=%u, sample_rate=%.0f, bandwidth=%.0f, fft_size=%u\n",
                pfb_config.num_channels, pfb_config.sample_rate,
                pfb_config.channel_bandwidth, pfb_config.fft_size);

        // Try to create with minimal config to isolate the issue
        pfb_config_t debug_config;
        if (pfb_config_init(&debug_config, 4, 1000000.0, 100000.0)) {
            debug_config.fft_size = 1024; // Smaller FFT
            pfb_t *debug_pfb = pfb_create(&debug_config);
            if (debug_pfb) {
                printf("  Debug: Minimal config works, issue with original config\n");
                pfb_destroy(debug_pfb);
            } else {
                printf("  Debug: Even minimal config fails - likely FFT issue\n");
            }
        }

        iq_free(&iq_data);
        return false;
    }

    if (options->verbose) {
        double isolation = pfb_calculate_isolation(&pfb_config);
        printf("📊 Expected channel isolation: %.1f dB\n", isolation);
    }

    // Process IQ data through channelizer
    uint32_t samples_processed = 0;
    uint32_t total_samples = iq_data.num_samples;
    const uint32_t block_size = 4096; // Process in blocks

    while (samples_processed < total_samples) {
        uint32_t samples_to_process = block_size;
        if (samples_processed + samples_to_process > total_samples) {
            samples_to_process = total_samples - samples_processed;
        }

        // Convert to complex float for PFB processing
        float complex *input_block = (float complex *)malloc(samples_to_process * sizeof(float complex));
        if (!input_block) {
            fprintf(stderr, "ERROR: Memory allocation failed\n");
            break;
        }

        // Convert IQ data to complex format
        for (uint32_t i = 0; i < samples_to_process; i++) {
            uint32_t idx = samples_processed + i;
            if (idx < iq_data.num_samples) {
                // Convert to float complex for PFB processing
                float real = iq_data.data[idx * 2];
                float imag = iq_data.data[idx * 2 + 1];
                ((float complex *)input_block)[i] = real + I * imag;
            }
        }

        // Process block through PFB
        int32_t processed = pfb_process_block(pfb, (const float complex *)input_block, samples_to_process);
        free(input_block);

        if (processed < 0) {
            fprintf(stderr, "ERROR: PFB processing failed\n");
            break;
        }

        samples_processed += processed;

        if (options->verbose && samples_processed % 100000 == 0) {
            printf("⏳ Processed %u/%u samples (%.1f%%)\n",
                   samples_processed, total_samples,
                   100.0 * samples_processed / total_samples);
        }
    }

    if (options->verbose) {
        printf("✅ Processing complete: %u samples processed\n", samples_processed);
    }

    // Extract and save channels
    for (uint32_t chan = 0; chan < options->num_channels; chan++) {
        uint32_t samples_available;
        const float complex *channel_data = pfb_get_channel_output(pfb, chan, &samples_available);

        if (samples_available > 0) {
            // Create channel filename
            char channel_filename[512];
            snprintf(channel_filename, sizeof(channel_filename), "%s/channel_%02u.iq",
                     options->output_dir, chan);

            // Convert back to IQ format and save
            float *iq_output = (float *)malloc(samples_available * 2 * sizeof(float));
            if (iq_output) {
                for (uint32_t i = 0; i < samples_available; i++) {
                    iq_output[i * 2] = crealf(channel_data[i]);
                    iq_output[i * 2 + 1] = cimagf(channel_data[i]);
                }

                // Create IQ data structure for saving
                iq_data_t channel_iq = {
                    .data = iq_output,
                    .num_samples = samples_available,
                    .sample_rate = options->sample_rate,
                    .format = strcmp(options->format, "s8") == 0 ? IQ_FORMAT_S8 : IQ_FORMAT_S16
                };

                bool save_success = iq_data_save_file(channel_filename, &channel_iq);

                free(iq_output);

                if (save_success) {
                    // Generate metadata
                    if (generate_channel_metadata(options->output_dir, chan, options, pfb)) {
                        if (options->verbose) {
                            printf("💾 Saved channel %u: %u samples, center=%.0f Hz\n",
                                   chan, samples_available,
                                   pfb_get_channel_frequency(pfb, chan));
                        }
                    } else {
                        fprintf(stderr, "WARNING: Failed to generate metadata for channel %u\n", chan);
                    }
                } else {
                    fprintf(stderr, "ERROR: Failed to save channel %u\n", chan);
                }
            }

            // Reset channel buffer
            pfb_reset_channel_output(pfb, chan);
        }
    }

    // Cleanup
    pfb_destroy(pfb);
    iq_free(&iq_data);

    if (options->verbose) {
        printf("🎉 Channelization complete! Files saved to: %s/\n", options->output_dir);
    }

    return true;
}

/**
 * @brief Main entry point
 */
int main(int argc, char **argv) {
    iqchan_options_t options;

    // Parse command-line arguments
    if (!parse_arguments(argc, argv, &options)) {
        return EXIT_FAILURE;
    }

    // Handle help
    if (options.help) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    // Validate options
    if (!validate_options(&options)) {
        return EXIT_FAILURE;
    }

    if (options.verbose) {
        printf("🚀 IQ Lab iqchan - Starting channelization...\n");
        printf("📥 Input: %s (%s format, %.0f Hz)\n",
               options.input_file, options.format, options.sample_rate);
        printf("📤 Output: %s/ (%u channels, %.0f Hz each)\n",
               options.output_dir, options.num_channels, options.channel_bandwidth);
    }

    // Process the file
    bool success = process_file(&options);

    if (success) {
        if (options.verbose) {
            printf("✅ Channelization completed successfully!\n");
        }
        return EXIT_SUCCESS;
    } else {
        fprintf(stderr, "❌ Channelization failed!\n");
        return EXIT_FAILURE;
    }
}
