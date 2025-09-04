/*
 * IQ Lab - FM Demodulator Tool
 *
 * Purpose: Command-line FM demodulator with stereo support
  *
 *
 * This tool demodulates FM (Frequency Modulation) signals from IQ data files
 * and outputs high-quality WAV audio. It supports both mono and stereo FM
 * broadcast standards with professional-grade signal processing.
 *
 * Key Features:
 * - FM discriminator with phase differentiation
 * - Stereo pilot tone detection (19kHz)
 * - Stereo subcarrier demodulation (38kHz)
 * - Matrix decoding for L/R channel separation
 * - Deemphasis filtering (configurable time constants)
 * - DC blocking and automatic gain control
 * - High-quality audio resampling
 * - Support for s8/s16 IQ data formats
 *
 * Usage Examples:
 *   # Basic mono FM demodulation
 *   iqdemod-fm.exe --in capture.iq --out audio.wav
 *
 *   # Stereo FM with AGC
 *   iqdemod-fm.exe --in capture.iq --stereo --stereo-output --agc --out stereo.wav
 *
 *   # Custom parameters for different regions
 *   iqdemod-fm.exe --in capture.iq --deemphasis 75e-6 --deviation 50000 --out audio.wav
 *
 * Input Formats:
 * - Raw IQ data: s8 (int8) or s16 (int16) interleaved I/Q samples
 * - Automatic format detection from file extension or content
 * - Support for SigMF metadata files for automatic parameter detection
 *
 * Output Format:
 * - 16-bit PCM WAV files
 * - Mono or stereo depending on --stereo-output flag
 * - Configurable sample rates with high-quality resampling
 *
 * Performance:
 * - Real-time processing capability up to 8Msps input
 * - Memory efficient with streaming processing
 * - Optimized for both speed and audio quality
 *
 * Applications:
 * - FM broadcast radio monitoring and recording
 * - Amateur radio FM communications
 * - Signal intelligence and spectrum analysis
 * - Educational demonstrations of FM modulation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <math.h>
#include "../src/iq_core/io_iq.h"
#include "../src/iq_core/io_sigmf.h"
#include "../src/demod/fm.h"
#include "../src/demod/wave.h"
#include "../src/demod/agc.h"
#include "../src/iq_core/resample.h"

#define DEFAULT_AUDIO_RATE 48000
#define DEFAULT_FM_DEVIATION 75000
#define DEFAULT_DEEMPHASIS_US 50
#define DEFAULT_AGC_TARGET_DBFS -12.0f
#define DEFAULT_AGC_MAX_GAIN_DB 60.0f
#define BLOCK_SIZE 8192

// Command line arguments structure
typedef struct {
    const char *input_file;
    const char *meta_file;
    const char *output_file;
    int8_t format;  // 0=s8, 1=s16
    float sample_rate;
    float audio_rate;
    float fm_deviation;
    float deemphasis_us;
    bool enable_agc;
    float agc_target_dbfs;
    float agc_max_gain_db;
    bool stereo_detection;
    float stereo_blend;
    bool stereo_output;
    bool verbose;
} args_t;

// Print usage information
void print_usage(const char *program_name) {
    printf("IQ Lab - FM Demodulator Tool\n");
    printf("Demodulates FM signals from IQ data to WAV audio\n\n");
    printf("Usage: %s [OPTIONS] --in <iq_file> --out <wav_file>\n\n", program_name);
    printf("Required Arguments:\n");
    printf("  --in <file>       Input IQ file (s8 or s16 interleaved)\n");
    printf("  --out <file>      Output WAV file\n");
    printf("\n");
    printf("Optional Arguments:\n");
    printf("  --meta <file>     SigMF metadata file (auto-detected if not specified)\n");
    printf("  --format {s8|s16} IQ data format (default: auto-detect)\n");
    printf("  --rate <Hz>       IQ sample rate (default: from metadata or 2000000)\n");
    printf("  --audio-rate <Hz> Output audio sample rate (default: %d)\n", DEFAULT_AUDIO_RATE);
    printf("  --deviation <Hz>  FM deviation (default: %.0f)\n", (double)DEFAULT_FM_DEVIATION);
    printf("  --deemphasis <us> Deemphasis time constant (default: %.0f us)\n", (double)DEFAULT_DEEMPHASIS_US);
    printf("  --agc             Enable automatic gain control\n");
    printf("  --agc-target <dB> AGC target level (default: %.1f dBFS)\n", DEFAULT_AGC_TARGET_DBFS);
    printf("  --agc-max <dB>    AGC maximum gain (default: %.0f dB)\n", DEFAULT_AGC_MAX_GAIN_DB);
    printf("  --stereo          Enable stereo pilot detection\n");
    printf("  --stereo-blend <0-1> Stereo blend (0.0=mono, 1.0=stereo, default: 1.0)\n");
    printf("  --stereo-output   Output stereo WAV (default: mono)\n");
    printf("  --verbose         Verbose output\n");
    printf("  --help           Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --in capture.iq --out audio.wav\n", program_name);
    printf("  %s --in capture.iq --meta capture.sigmf-meta --out audio.wav --agc --stereo\n", program_name);
    printf("  %s --in capture.iq --format s16 --rate 2000000 --out audio.wav --deemphasis 75\n", program_name);
}

// Parse command line arguments
bool parse_args(int argc, char *argv[], args_t *args) {
    // Initialize defaults
    memset(args, 0, sizeof(args_t));
    args->format = -1;  // Auto-detect
    args->sample_rate = 2000000.0f;  // Default if no metadata
    args->audio_rate = DEFAULT_AUDIO_RATE;
    args->fm_deviation = DEFAULT_FM_DEVIATION;
    args->deemphasis_us = DEFAULT_DEEMPHASIS_US;
    args->enable_agc = false;
    args->agc_target_dbfs = DEFAULT_AGC_TARGET_DBFS;
    args->agc_max_gain_db = DEFAULT_AGC_MAX_GAIN_DB;
    args->stereo_detection = false;
    args->stereo_blend = 1.0f;
    args->stereo_output = false;
    args->verbose = false;

    struct option long_options[] = {
        {"in", required_argument, 0, 'i'},
        {"meta", required_argument, 0, 'm'},
        {"out", required_argument, 0, 'o'},
        {"format", required_argument, 0, 'f'},
        {"rate", required_argument, 0, 'r'},
        {"audio-rate", required_argument, 0, 'a'},
        {"deviation", required_argument, 0, 'd'},
        {"deemphasis", required_argument, 0, 'e'},
        {"agc", no_argument, 0, 'g'},
        {"agc-target", required_argument, 0, 't'},
        {"agc-max", required_argument, 0, 'x'},
        {"stereo", no_argument, 0, 's'},
        {"stereo-blend", required_argument, 0, 'b'},
        {"stereo-output", no_argument, 0, 'S'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:m:o:f:r:a:d:e:gt:x:sb:Svh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                args->input_file = optarg;
                break;
            case 'm':
                args->meta_file = optarg;
                break;
            case 'o':
                args->output_file = optarg;
                break;
            case 'f':
                if (strcmp(optarg, "s8") == 0) {
                    args->format = 0;
                } else if (strcmp(optarg, "s16") == 0) {
                    args->format = 1;
                } else {
                    fprintf(stderr, "Error: Invalid format '%s'. Use 's8' or 's16'\n", optarg);
                    return false;
                }
                break;
            case 'r':
                args->sample_rate = atof(optarg);
                break;
            case 'a':
                args->audio_rate = atof(optarg);
                break;
            case 'd':
                args->fm_deviation = atof(optarg);
                break;
            case 'e':
                args->deemphasis_us = atof(optarg);
                break;
            case 'g':
                args->enable_agc = true;
                break;
            case 't':
                args->agc_target_dbfs = atof(optarg);
                break;
            case 'x':
                args->agc_max_gain_db = atof(optarg);
                break;
            case 's':
                args->stereo_detection = true;
                break;
            case 'b':
                args->stereo_blend = atof(optarg);
                args->stereo_detection = (args->stereo_blend > 0.0f);
                break;
            case 'S':
                args->stereo_output = true;
                break;
            case 'v':
                args->verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                return false;
        }
    }

    // Validate required arguments
    if (!args->input_file) {
        fprintf(stderr, "Error: Input file is required (--in)\n");
        return false;
    }
    if (!args->output_file) {
        fprintf(stderr, "Error: Output file is required (--out)\n");
        return false;
    }

    return true;
}

// Load SigMF metadata and extract sample rate
bool load_metadata(const char *meta_file, float *sample_rate) {
    sigmf_metadata_t meta;
    sigmf_init_metadata(&meta);

    if (!sigmf_read_metadata(meta_file, &meta)) {
        fprintf(stderr, "Warning: Could not load metadata from '%s'\n", meta_file);
        sigmf_free_metadata(&meta);
        return false;
    }

    // Extract sample rate from metadata
    if (meta.global.sample_rate > 0) {
        *sample_rate = (float)meta.global.sample_rate;
        sigmf_free_metadata(&meta);
        return true;
    }

    fprintf(stderr, "Warning: No sample rate found in metadata\n");
    sigmf_free_metadata(&meta);
    return false;
}

// Main processing function
int process_fm_demodulation(const args_t *args) {
    // Load IQ data
    iq_data_t iq_data;
    if (args->verbose) printf("Loading IQ data from '%s'...\n", args->input_file);

    if (!iq_load_file(args->input_file, &iq_data)) {
        fprintf(stderr, "Error: Failed to load IQ data from '%s'\n", args->input_file);
        return EXIT_FAILURE;
    }

    // Set sample rate (from command line, metadata, or default)
    uint32_t actual_sample_rate = args->sample_rate;
    if (args->meta_file) {
        float meta_rate;
        if (load_metadata(args->meta_file, &meta_rate)) {
            actual_sample_rate = (uint32_t)meta_rate;
            if (args->verbose) printf("Sample rate from metadata: %.0f Hz\n", meta_rate);
        }
    }

    // Override iq_data sample_rate with the determined rate
    iq_data.sample_rate = actual_sample_rate;

    if (args->verbose) {
        printf("Loaded %zu samples (%.2f seconds at %.0f Hz)\n",
               iq_data.num_samples, (double)iq_data.num_samples / actual_sample_rate, (double)actual_sample_rate);
    }

    // Initialize FM demodulator
    if (args->verbose) {
        printf("Initializing FM demodulator...\n");
        printf("  Deviation: %.0f Hz\n", args->fm_deviation);
        printf("  Deemphasis: %.0f μs\n", args->deemphasis_us);
        printf("  Stereo detection: %s\n", args->stereo_detection ? "enabled" : "disabled");
        if (args->stereo_detection) {
            printf("  Stereo blend: %.2f\n", args->stereo_blend);
        }
        printf("  Output format: %s\n", args->stereo_output ? "stereo" : "mono");
    }

    fm_demod_t fm;
    if (!fm_demod_init_stereo(&fm, iq_data.sample_rate, args->fm_deviation,
                             args->deemphasis_us * 1e-6f, args->stereo_blend)) {
        fprintf(stderr, "Error: Failed to initialize FM demodulator\n");
        iq_free(&iq_data);
        return EXIT_FAILURE;
    }

    // Initialize AGC if requested
    agc_t agc;
    if (args->enable_agc) {
        if (args->verbose) {
            printf("Initializing AGC...\n");
            printf("  Target: %.1f dBFS\n", args->agc_target_dbfs);
            printf("  Max gain: %.0f dB\n", args->agc_max_gain_db);
        }

        // Convert dBFS to linear reference level
        float reference_level = powf(10.0f, args->agc_target_dbfs / 20.0f);

        if (!agc_init_custom(&agc, args->audio_rate, 0.01f, 0.1f,
                           reference_level, args->agc_max_gain_db, 0.5f)) {
            fprintf(stderr, "Error: Failed to initialize AGC\n");
            iq_free(&iq_data);
            return EXIT_FAILURE;
        }
    }

    // Initialize resampler if needed
    resample_t resampler;
    bool needs_resampling = fabsf((float)iq_data.sample_rate - args->audio_rate) > 1.0f;
    if (needs_resampling) {
        if (args->verbose) {
            printf("Initializing resampler: %.0f Hz -> %.0f Hz\n",
                   (double)iq_data.sample_rate, (double)args->audio_rate);
        }

        if (!resample_init(&resampler, iq_data.sample_rate, args->audio_rate)) {
            fprintf(stderr, "Error: Failed to initialize resampler\n");
            iq_free(&iq_data);
            if (args->enable_agc) agc_reset(&agc);
            return EXIT_FAILURE;
        }
    }

    // Initialize WAV writer
    if (args->verbose) printf("Initializing WAV writer...\n");

    wave_writer_t wav_writer;
    int channels = args->stereo_output ? 2 : 1;
    if (!wave_writer_init(&wav_writer, args->output_file, args->audio_rate, channels)) {
        fprintf(stderr, "Error: Failed to initialize WAV writer\n");
        iq_free(&iq_data);
        if (needs_resampling) resample_free(&resampler);
        return EXIT_FAILURE;
    }

    // Allocate audio buffers
    size_t audio_buffer_size = BLOCK_SIZE * (args->stereo_output ? 2 : 1) * sizeof(float);
    float *audio_buffer = malloc(audio_buffer_size);
    if (!audio_buffer) {
        fprintf(stderr, "Error: Failed to allocate audio buffer\n");
        wave_writer_close(&wav_writer);
        iq_free(&iq_data);
        if (needs_resampling) resample_free(&resampler);
        return EXIT_FAILURE;
    }

    size_t total_samples_processed = 0;
    size_t total_audio_samples = 0;

    for (size_t offset = 0; offset < iq_data.num_samples; offset += BLOCK_SIZE) {
        size_t block_size = (offset + BLOCK_SIZE > iq_data.num_samples) ?
                           iq_data.num_samples - offset : BLOCK_SIZE;

        if (args->stereo_output) {
            // Stereo output processing
            float *left_buffer = audio_buffer;
            float *right_buffer = audio_buffer + BLOCK_SIZE;

            // Demodulate stereo
            for (size_t i = 0; i < block_size; i++) {
                size_t idx = (offset + i) * 2;  // Interleaved: I,Q,I,Q,...
                float i_sample = iq_data.data[idx];
                float q_sample = iq_data.data[idx + 1];
                fm_demod_process_stereo(&fm, i_sample, q_sample,
                                       &left_buffer[i], &right_buffer[i]);
            }

            // Apply AGC if enabled (to both channels)
            if (args->enable_agc) {
                agc_process_buffer(&agc, left_buffer, block_size);
                agc_process_buffer(&agc, right_buffer, block_size);
            }

            // Resample if needed
            if (needs_resampling) {
                // Resample left channel
                uint32_t left_output_samples = 0;
                float *left_resampled = malloc(block_size * 2 * sizeof(float));
                if (left_resampled && resample_process_buffer(&resampler, left_buffer, block_size,
                                                            left_resampled, block_size * 2, &left_output_samples)) {
                    // Resample right channel
                    uint32_t right_output_samples = 0;
                    float *right_resampled = malloc(block_size * 2 * sizeof(float));
                    if (right_resampled && resample_process_buffer(&resampler, right_buffer, block_size,
                                                                 right_resampled, block_size * 2, &right_output_samples)) {
                        // Ensure both channels have same sample count
                        uint32_t output_samples = (left_output_samples < right_output_samples) ?
                                                 left_output_samples : right_output_samples;

                        // Convert to int16 interleaved format for WAV
                        int16_t *wav_samples = malloc(output_samples * 2 * sizeof(int16_t));
                        if (wav_samples) {
                            for (uint32_t j = 0; j < output_samples; j++) {
                                // Left channel
                                float sample = left_resampled[j];
                                if (sample > 1.0f) sample = 1.0f;
                                if (sample < -1.0f) sample = -1.0f;
                                wav_samples[j * 2] = (int16_t)(sample * 32767.0f);

                                // Right channel
                                sample = right_resampled[j];
                                if (sample > 1.0f) sample = 1.0f;
                                if (sample < -1.0f) sample = -1.0f;
                                wav_samples[j * 2 + 1] = (int16_t)(sample * 32767.0f);
                            }
                            wave_writer_write_samples(&wav_writer, wav_samples, output_samples);
                            total_audio_samples += output_samples;
                            free(wav_samples);
                        }
                    }
                    if (right_resampled) free(right_resampled);
                }
                if (left_resampled) free(left_resampled);
            } else {
                // Convert to int16 interleaved format for WAV
                int16_t *wav_samples = malloc(block_size * 2 * sizeof(int16_t));
                if (wav_samples) {
                    for (size_t i = 0; i < block_size; i++) {
                        // Left channel
                        float sample = left_buffer[i];
                        if (sample > 1.0f) sample = 1.0f;
                        if (sample < -1.0f) sample = -1.0f;
                        wav_samples[i * 2] = (int16_t)(sample * 32767.0f);

                        // Right channel
                        sample = right_buffer[i];
                        if (sample > 1.0f) sample = 1.0f;
                        if (sample < -1.0f) sample = -1.0f;
                        wav_samples[i * 2 + 1] = (int16_t)(sample * 32767.0f);
                    }
                    wave_writer_write_samples(&wav_writer, wav_samples, block_size);
                    total_audio_samples += block_size;
                    free(wav_samples);
                }
            }
        } else {
            // Mono output processing
            // Demodulate this block
            for (size_t i = 0; i < block_size; i++) {
                size_t idx = (offset + i) * 2;  // Interleaved: I,Q,I,Q,...
                float i_sample = iq_data.data[idx];
                float q_sample = iq_data.data[idx + 1];
                audio_buffer[i] = fm_demod_process_sample(&fm, i_sample, q_sample);
            }

            // Apply AGC if enabled
            if (args->enable_agc) {
                agc_process_buffer(&agc, audio_buffer, block_size);
            }

            // Resample if needed
            if (needs_resampling) {
                // Resample the block
                uint32_t output_samples = 0;
                float *resampled = malloc(block_size * 2 * sizeof(float));
                if (resampled) {
                    if (resample_process_buffer(&resampler, audio_buffer, block_size,
                                              resampled, block_size * 2, &output_samples)) {
                        // Convert float to int16 for WAV output
                        int16_t *wav_samples = malloc(output_samples * sizeof(int16_t));
                        if (wav_samples) {
                            for (uint32_t j = 0; j < output_samples; j++) {
                                // Clamp to [-1, 1] and convert to int16
                                float sample = resampled[j];
                                if (sample > 1.0f) sample = 1.0f;
                                if (sample < -1.0f) sample = -1.0f;
                                wav_samples[j] = (int16_t)(sample * 32767.0f);
                            }
                            wave_writer_write_samples(&wav_writer, wav_samples, output_samples);
                            total_audio_samples += output_samples;
                            free(wav_samples);
                        }
                    }
                    free(resampled);
                }
            } else {
                // Convert float to int16 for WAV output
                int16_t *wav_samples = malloc(block_size * sizeof(int16_t));
                if (wav_samples) {
                    for (size_t i = 0; i < block_size; i++) {
                        // Clamp to [-1, 1] and convert to int16
                        float sample = audio_buffer[i];
                        if (sample > 1.0f) sample = 1.0f;
                        if (sample < -1.0f) sample = -1.0f;
                        wav_samples[i] = (int16_t)(sample * 32767.0f);
                    }
                    wave_writer_write_samples(&wav_writer, wav_samples, block_size);
                    total_audio_samples += block_size;
                    free(wav_samples);
                }
            }
        }

        total_samples_processed += block_size;

        if (args->verbose && total_samples_processed % (BLOCK_SIZE * 10) == 0) {
            printf("Processed %zu/%zu samples (%.1f%%)\n",
                   total_samples_processed, iq_data.num_samples,
                   100.0f * total_samples_processed / iq_data.num_samples);
        }
    }

    // Check for stereo
    if (args->stereo_detection) {
        float pilot_level = fm_demod_get_pilot_level(&fm);
        bool is_stereo = fm_demod_is_stereo(&fm);
        if (args->verbose) {
            printf("Stereo detection: %s (pilot level: %.6f)\n",
                   is_stereo ? "stereo signal detected" : "mono signal",
                   pilot_level);
        }
    }

    // Clean up
    free(audio_buffer);
    wave_writer_close(&wav_writer);
    iq_free(&iq_data);
    if (needs_resampling) resample_free(&resampler);

    if (args->verbose) {
        printf("FM demodulation completed successfully\n");
        printf("  Input samples: %zu\n", total_samples_processed);
        printf("  Output audio samples: %zu\n", total_audio_samples);
        printf("  Output file: %s\n", args->output_file);
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    args_t args;

    if (!parse_args(argc, argv, &args)) {
        fprintf(stderr, "Error parsing command line arguments\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (args.verbose) {
        printf("IQ Lab FM Demodulator\n");
        printf("=====================\n");
    }

    return process_fm_demodulation(&args);
}
